// Copyright 2022 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// A simple utility that prints the names of the problems in a dataset. If
// provided multiple filenames as arguments, these are read sequentially.

#include <fcntl.h>

#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>
#include <sstream>
#include <filesystem>

#include "absl/flags/parse.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "contest_problem.pb.h"
#include "execution/py_locations.h"
#include "execution/py_tester_sandboxer.h"
#include "execution/status_macros.h"
#include "execution/tester_sandboxer.h"
#include "riegeli/bytes/fd_reader.h"
#include "riegeli/records/record_reader.h"

// For .json processing
#include "nlohmann/json.hpp"
#include <fstream>
using json = nlohmann::json;

ABSL_FLAG(std::string, test_path, "", "Path to test dataset.");
ABSL_FLAG(std::string, output_dir, "", "Where the .json with results should be saved.");

namespace deepmind::code_contests {
namespace {

int cnt_crashed_tests;
int cnt_passed_tests;
int cnt_failed_tests;
bool debug = true;
json results;
// json single_problem_results;
json all_problem_samples;
json test_results;
json solutions;
std::string soln_lang;

absl::StatusOr<ContestProblem> FindProblem(
    const absl::string_view filename, std::string target_problem_name) {
  riegeli::RecordReader<riegeli::FdReader<>> reader(
      std::forward_as_tuple(filename));
  ContestProblem problem;
  while (reader.ReadRecord(problem)) {
    if (problem.name() == target_problem_name) return problem;
  }
  std::cout << "Problem " << target_problem_name;
  return absl::NotFoundError(
      " not found inside of the test dataset");
}

std::vector<absl::string_view> GetInputs(const ContestProblem& problem,
                                         int max_size) {
  std::vector<absl::string_view> inputs;
  for (const auto& test : problem.public_tests()) {
    inputs.push_back(test.input());
  }
  for (const auto& test : problem.private_tests()) {
    inputs.push_back(test.input());
  }
  for (const auto& test : problem.generated_tests()) {
    inputs.push_back(test.input());
  }
  if (debug == true) {
    inputs.resize(max_size);
  }

  inputs.resize(max_size);
  return inputs;
}

std::vector<absl::string_view> GetOutputs(const ContestProblem& problem,
                                          int max_size) {
  std::vector<absl::string_view> outputs;
  for (const auto& test : problem.public_tests()) {
    outputs.push_back(test.output());
  }
  for (const auto& test : problem.private_tests()) {
    outputs.push_back(test.output());
  }
  for (const auto& test : problem.generated_tests()) {
    outputs.push_back(test.output());
  }
  if (debug == true) {
    outputs.resize(max_size);
  }
  outputs.resize(max_size);
  return outputs;
}

void ReportResults(const MultiTestResult& multi_result) {
  if (debug==true) {
    std::cout << "Compilation "  
              << (multi_result.compilation_result.program_status ==
                          ProgramStatus::kSuccess
                      ? "succeeded"
                      : "failed")
              //<< "\nThe stdout output was:\n"
              //<< (multi_result.compilation_result.stdout)
              //<< "\nThe stderr output was:\n"
              //<< (multi_result.compilation_result.stderr);
              << "\n";
  }

  int i = 0;

  // Tallying up the test results
  cnt_crashed_tests = 0;
  cnt_passed_tests = 0;
  cnt_failed_tests = 0;

  for (const auto& test_result : multi_result.test_results) {
    if (!test_result.passed.has_value()) {
      // std::cout << "Test " << i << " did not run.\n";
      ++cnt_crashed_tests;
    } else if (*test_result.passed) {
      // std::cout << "Test " << i << " passed.\n";
      ++cnt_passed_tests;
    } else {
      // std::cout << "Test " << i << " failed.\n";
      ++cnt_failed_tests;
    }
    ++i;
  }

  if (debug == true){
  std::cout << "Tests     ";
  std::cout << "Passed: " << cnt_passed_tests << "/" << i << "   ";
  std::cout << "Failed: " << cnt_failed_tests << "/" << i << "   ";
  std::cout << "Crashed: " << cnt_crashed_tests << "/" << i << "\n";
  }
}

json calculate_metrics(json single_problem) {

  int n; // for counting the sample size for each problem
  int c; // for counting the amount of passes in each sample

  n = 0;
  c = 0;
  for (json solution : single_problem["test_results"]) {
    if (solution["passed_all_tests"] == true) {
      ++c;
    }
    ++n;
  }
  // std::cout << "\n" << problem["problem"] << ":\nn = " \
  //   << n << ", c = " << c << "\n\n";

  bool pass = false;

  if (c >= 0) {
    pass = true;
  }

  single_problem["test_metrics"]["pass_at_k"] = pass;
  single_problem["test_metrics"]["sample_size"] = n;
  single_problem["test_metrics"]["number_passes"] = c;

  return single_problem;
}

absl::Status SolveProblem(
    const absl::string_view test_filename) {

  std::string problem_name = solutions["problem_name"].get<std::string>();
  
  ASSIGN_OR_RETURN(ContestProblem problem_being_solved,
                   FindProblem(test_filename, problem_name));
  const std::vector<absl::string_view> inputs =
      GetInputs(problem_being_solved,
                /*max_size=*/3);
  const std::vector<absl::string_view> outputs =
      GetOutputs(problem_being_solved,
                 /*max_size=*/3);


  Py3TesterSandboxer tester(Py3InterpreterPath(), Py3LibraryPaths());
  TestOptions options;
  options.num_threads = 4;
  options.stop_on_first_failure = true;

  std::cout << "\n Working on problem: '" << problem_name << "'\n";

  if (debug == true){
  std::cout << R"(Trying to solve the selected problem.

There are 3 options for the outcome of the tests:
  1. (passed) The program runs successfully and gives the correct answer in all the tests.
  2. (failed) The program runs successfully, but gives the wrong answer sometimes.
  3. (crashed) The program does not compile.

)";
  }
  
  json single_problem_results;
  single_problem_results["problem"] = problem_name;

  int i = 0;
  std::string soln_correct;
  for (json soln : solutions["generated_solutions"]) {
  

    soln_lang = soln["language"].get<std::string>();
    if (soln["is_correct"].get<bool>() == true) {
      soln_correct = "correct";
    } else {
      soln_correct = "incorrect";
    }
    // absl::string_view soln_code = soln["code"].get<absl::string_view>();
    // std::cout << "\n\n\nSolution " << i << ", code (" << soln_lang << "):\n-------------------------\n" << soln_code;
    if (soln_lang == "python3") {
      absl::string_view soln_code = soln["code"].get<absl::string_view>();
      ASSIGN_OR_RETURN(MultiTestResult result_output,
                    tester.Test(soln_code, inputs, options, outputs));
      if (debug == true) {
        std::cout << "\nSolution " << i << " (" << soln_lang <<", " << soln_correct << "): ";
      }
      ReportResults(result_output);

      test_results["solution_number"] = i;
      test_results["language"] = soln_lang;
      test_results["tests_passed"] = cnt_passed_tests;
      test_results["tests_failed"] = cnt_failed_tests;
      test_results["tests_crashed"] = cnt_crashed_tests;

      // if passed all unit tests, count that as a pass in pass@k
      if (cnt_passed_tests == cnt_passed_tests + cnt_failed_tests + cnt_crashed_tests) {
        test_results["passed_all_tests"] = true;
      } else {
        test_results["passed_all_tests"] = false;
      }

      
      single_problem_results["test_results"].push_back(test_results);

      

      // if (debug == true) {
      //   std::cout << "\n" << results << "\n";
      // }

    }
    ++i;
    
  }

  single_problem_results = calculate_metrics(single_problem_results);

  // exclude from output if no solutions in supported language were found
  if (single_problem_results.size() != 1) {
    results.push_back(single_problem_results);
    std::cout << "\nNo solutions in a supported language were found!\n";
  }
  

  return absl::OkStatus();
}

}  // namespace
}  // namespace deepmind::code_contests


int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);

  std::ifstream sample_solutions_file("/home/maksgepner/CodeGenerationAnalysis/CodeContests/execution/sample_solutions.jsonl");
	json single_problem;
  std::string line;

  while (std::getline(sample_solutions_file, line)) {
    single_problem = json::parse(line);
    // std::cout << "\n" << single_problem["generated_solutions"].front() << "\n";


  // sample_solutions_file >> deepmind::code_contests::solutions;
  // sample_solutions_file >> deepmind::code_contests::all_problem_samples;

  // std::cout << "\n" << deepmind::code_contests::all_problem_samples["problem_name"] << "\n";

    deepmind::code_contests::solutions = single_problem;

    // std::cout << "\n" << single_problem.front() << "\n";

    if (absl::Status status = deepmind::code_contests::SolveProblem(
          absl::GetFlag(FLAGS_test_path));
      !status.ok()) {
    std::cerr << "Failed: " << status.message() << std::endl;
    }
  }

  // deepmind::code_contests::calculate_metrics();
  
  // Export the results
  std::string output_filename = "test_results.json";

  std::string output_path = absl::GetFlag(FLAGS_output_dir) + output_filename;
  std::ofstream OutputFile("/home/maksgepner/CodeGenerationAnalysis/CodeContests/execution/test_results.json");
  OutputFile << deepmind::code_contests::results;
  OutputFile.close();

}
