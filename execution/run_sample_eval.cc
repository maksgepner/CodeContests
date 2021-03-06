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
#include <algorithm>
#include <random>
#include <iterator>

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

// Option for debugging
bool debug = false;
bool fast_run = false;


json results;
// json single_problem_results;
json all_problem_samples;
json test_results;
json solutions;
std::string soln_lang;
int num_public_tests;
int cnt_passed_public_tests;

int number_passed_problems = 0;
int number_passed_ten_at_k_problems = 0;
int number_evaluated_problems = 0;

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

  // Used to find which solutions passed all public tests (for 10@k clustering)
  num_public_tests = inputs.size();
  // std::cout << "\nThe number of public_tests is: " << inputs.size()  << "\n";

  for (const auto& test : problem.private_tests()) {
    inputs.push_back(test.input());
  }
  for (const auto& test : problem.generated_tests()) {
    inputs.push_back(test.input());
  }
  if (fast_run == true) {
    inputs.resize(max_size);
  }

  // inputs.resize(max_size);
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
  if (fast_run == true) {
    outputs.resize(max_size);
  }
  // outputs.resize(max_size);
  return outputs;
}

void ReportResults(const MultiTestResult& multi_result) {
  if (debug==true) {
    std::cout << "Compilation "  
              << (multi_result.compilation_result.program_status ==
                          ProgramStatus::kSuccess
                      ? "succeeded"
                      : "failed")
              << "\nThe compilation stdout output was:\n"
              << (multi_result.compilation_result.stdout)
              << "\nThe compilation stderr output was:\n"
              << (multi_result.compilation_result.stderr)
              // << "\nThe runtime output was:\n"
              // << (multi_result.test_results)
              // << "\nThe runtime stderr output was:\n"
              // << (multi_result.test_results.stderr)
              << "\n";
    int index = 0;
    for (const ExecutionResult& result : multi_result.test_results) {
      std::cout << "  Test Result " << index++ << ":\n";
      std::cout << result << "\n\n";
  }
  }

  int i = 0;

  // Tallying up the test results
  cnt_crashed_tests = 0;
  cnt_passed_tests = 0;
  cnt_failed_tests = 0;
  cnt_passed_public_tests = 0;

  for (const auto& test_result : multi_result.test_results) {
    if (!test_result.passed.has_value()) {
      // std::cout << "Test " << i << " did not run.\n";
      ++cnt_crashed_tests;
    } else if (*test_result.passed) {
      // std::cout << "Test " << i << " passed.\n";
      ++cnt_passed_tests;
      if (i < num_public_tests) {
        ++cnt_passed_public_tests;
      }
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

  int n_sample = 0; // for counting the sample size for each problem
  int c_passes = 0; // for counting the amount of passes in each sample
  std::vector<int> idx_passed_public_tests; // for sampling solns that passed public tests

  for (json solution : single_problem["test_results"]) {
    if (solution["passed_all_tests"] == true) {
      ++c_passes;
    }
    if (solution["passed_public_tests"] == true) {
      idx_passed_public_tests.push_back(n_sample);
    }
    ++n_sample;
  }

  // for (int idx : idx_passed_public_tests) {
  //   std::cout << "\n idx (passed) = " << idx << "\n";
  // }

  std::vector<int> idx_sample_ten_at_k;
  std::sample(idx_passed_public_tests.begin(), idx_passed_public_tests.end(), std::back_inserter(idx_sample_ten_at_k),
                10, std::mt19937{std::random_device{}()});

  // for (int idx : idx_sample_ten_at_k) {
  //   std::cout << "\n idx (sampled) = " << idx << "\n";
  // }

  int cnt = 0;
  int c_cluster = 0;
  for (json solution : single_problem["test_results"]) {
    if (std::find(idx_sample_ten_at_k.begin(), idx_sample_ten_at_k.end(), cnt) != idx_sample_ten_at_k.end()) {
      if (solution["passed_all_tests"] == true) {
        ++c_cluster;
      }
    }
    ++cnt;
  }

  std::cout << "\n" << single_problem["problem"] << ":\nn = " \
    << n_sample << ", c = " << c_passes << "\n\n";

  bool pass_at_k_passed = false;
  bool ten_at_k_passed = false;


  // pass@k = k@k (only 1 pass needed from the whole sample)
  if (c_passes > 0) {
    pass_at_k_passed = true;
    ++number_passed_problems;
  }
  single_problem["test_metrics"]["pass_at_k_passed"] = pass_at_k_passed;
  
  // 10@k ??? implementation of appropriate clustering method needed
  // (take 10 from the sample pool, then check if at least one passed tests
  if (c_cluster == idx_sample_ten_at_k.size() and n_sample != 0 and idx_sample_ten_at_k.size() != 0) {
    ten_at_k_passed = true;
    ++number_passed_ten_at_k_problems;
  }
  single_problem["test_metrics"]["ten_at_k_passed"] = ten_at_k_passed;

  
  single_problem["test_metrics"]["sample_size"] = n_sample;
  single_problem["test_metrics"]["number_passes"] = c_passes;

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
    // if (soln["is_correct"].get<bool>() == true) {
    //   soln_correct = "correct";
    // } else {
    //   soln_correct = "incorrect";
    // }
    
    // absl::string_view soln_code = soln["code"].get<absl::string_view>();
    // std::cout << "\n\n\nSolution " << i << ", code (" << soln_lang << "):\n-------------------------\n" << soln_code;
    if (soln_lang == "python3") {
      absl::string_view soln_code = soln["code"].get<absl::string_view>();
      ASSIGN_OR_RETURN(MultiTestResult result_output,
                    tester.Test(soln_code, inputs, options, outputs));
      if (debug == true) {
        std::cout << "\nSolution " << i << " (" << soln_lang << "): ";
        // std::cout << "\nSolution " << i << " (" << soln_lang <<", " << soln_correct << "): ";
      }
      ReportResults(result_output);

      test_results["solution_number"] = i;
      test_results["language"] = soln_lang;
      test_results["tests_passed"] = cnt_passed_tests;
      test_results["tests_failed"] = cnt_failed_tests;
      test_results["tests_crashed"] = cnt_crashed_tests;

      int cnt_ran_tests = cnt_passed_tests + cnt_failed_tests + cnt_crashed_tests;
      
      if (cnt_ran_tests == 0) {
        test_results["compilation"] = "fail";
      } else {
        test_results["compilation"] = "success";
      }
        


      // if passed all unit tests, count that as a pass in pass@k
      if (cnt_passed_tests == cnt_ran_tests and cnt_ran_tests != 0) {
        test_results["passed_all_tests"] = true;
      } else {
        test_results["passed_all_tests"] = false;
      }
      
      if (cnt_passed_public_tests == num_public_tests and num_public_tests != 0) {
        test_results["passed_public_tests"] = true;
      } else {
        test_results["passed_public_tests"] = false;
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
  if (single_problem_results["test_metrics"]["sample_size"] != 0) {
    results.push_back(single_problem_results);
    ++number_evaluated_problems;
  } else {
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

  int n;
  int k;
  int c;
  double codex_pass_at_1;
  double codex_pass_at_10;
  double codex_pass_at_100;
  double prod;
  double pass_at_k;
  double ten_at_k;
  std::string output_filename;
  std::string output_path;

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

    // Export the (intermediate) results
    output_filename = "test_results.json";
    output_path = absl::GetFlag(FLAGS_output_dir) + output_filename;
    std::ofstream OutputFile1(output_path);
    OutputFile1 << deepmind::code_contests::results;
    OutputFile1.close();

    n = deepmind::code_contests::number_evaluated_problems;
    k = deepmind::code_contests::number_evaluated_problems;
    c = deepmind::code_contests::number_passed_problems;

    pass_at_k = c / (double)k;
    ten_at_k = deepmind::code_contests::number_passed_ten_at_k_problems / (double)k;

    k = 1;
    if (n - c < k) {
      codex_pass_at_1 = 1.0;
    } else {
      prod = 1;
      for (double i = n - c + 1; i < n + 1; i++) {
        prod = prod * (1 - k / i);
        // std::cout << "\n Codex pass@k: " << 1 - prod << "\n";
      }
      codex_pass_at_1 = 1 - prod;
    }

    k = 10;
    if (n - c < k) {
      codex_pass_at_10 = 1.0;
    } else {
      prod = 1;
      for (double i = n - c + 1; i < n + 1; i++) {
        prod = prod * (1 - k / i);
        // std::cout << "\n Codex pass@k: " << 1 - prod << "\n";
      }
      codex_pass_at_10 = 1 - prod;
    }

    k = 100;
    if (n - c < k) {
      codex_pass_at_100 = 1.0;
    } else {
      prod = 1;
      for (double i = n - c + 1; i < n + 1; i++) {
        prod = prod * (1 - k / i);
        // std::cout << "\n Codex pass@k: " << 1 - prod << "\n";
      }
      codex_pass_at_100 = 1 - prod;
    }

    json result_metrics;
    result_metrics["k"] = deepmind::code_contests::number_evaluated_problems;
    result_metrics["c"] = c;
    result_metrics["c_cluster"] = deepmind::code_contests::number_passed_ten_at_k_problems;
    result_metrics["pass_at_k"] = pass_at_k;
    result_metrics["ten_at_k"] = pass_at_k;
    result_metrics["codex_pass_at_1"] = codex_pass_at_1;
    result_metrics["codex_pass_at_10"] = codex_pass_at_10;
    result_metrics["codex_pass_at_100"] = codex_pass_at_100;


    // Export the (intermediate) metrics
    output_filename = "test_metrics.json";
    output_path = absl::GetFlag(FLAGS_output_dir) + output_filename;
    std::ofstream OutputFile2(output_path);
    OutputFile2 << result_metrics;
    OutputFile2.close();

  }



  std::cout << "\n\n\nExperiments finished.\n";
  std::cout << "k = " << deepmind::code_contests::number_evaluated_problems << "\n";
  std::cout << "c = " << c << "\n";
  std::cout << "c_cluster = " << deepmind::code_contests::number_passed_ten_at_k_problems << "\n";
  std::cout << "Alphacode pass@k = " << pass_at_k << "\n";
  std::cout << "Alphacode 10@k = " << ten_at_k << "\n";
  std::cout << "Codex pass@1 = " << codex_pass_at_1 << "\n";
  std::cout << "Codex pass@10 = " << codex_pass_at_10 << "\n";
  std::cout << "Codex pass@100 = " << codex_pass_at_100 << "\n";
  

}
