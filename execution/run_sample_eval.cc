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
#include "json.h"
#include <fstream>

ABSL_FLAG(std::string, test_path, "", "Path to test dataset.");

namespace deepmind::code_contests {
namespace {

// absl::StatusOr<ContestProblem> FindProblem(
//     const absl::string_view filename) {
//   riegeli::RecordReader<riegeli::FdReader<>> reader(
//       std::forward_as_tuple(filename));
//   ContestProblem problem;
//   while (reader.ReadRecord(problem)) {
//     if (problem.name() == target_problem_name) return problem;
//   }
//   std::cout << "Problem " << target_problem_name
//   return absl::NotFoundError(
//       " not found inside of the test dataset");
// }

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
  outputs.resize(max_size);
  return outputs;
}

void ReportResults(const MultiTestResult& multi_result) {
  std::cout << "Compilation "  
            << (multi_result.compilation_result.program_status ==
                        ProgramStatus::kSuccess
                    ? "succeeded"
                    : "failed");
            //<< "\nThe stdout output was:\n"
            //<< (multi_result.compilation_result.stdout)
            //<< "\nThe stderr output was:\n"
            //<< (multi_result.compilation_result.stderr);
  int i = 0;
  for (const auto& test_result : multi_result.test_results) {
    if (!test_result.passed.has_value()) {
      std::cout << "Test " << i << " did not run.\n";
    } else if (*test_result.passed) {
      std::cout << "Test " << i << " passed.\n";
    } else {
      std::cout << "Test " << i << " failed.\n";
    }
    ++i;
  }
}

absl::Status SolveProblem(
    const absl::string_view test_filename) {
  // ASSIGN_OR_RETURN(ContestProblem problem_filename,
  //                  FindProblem(test_filename));
  // const std::vector<absl::string_view> inputs =
  //     GetInputs(problem_filename,
  //               /*max_size=*/10);
  // const std::vector<absl::string_view> outputs =
  //     GetOutputs(problem_filename,
  //                /*max_size=*/10);

  Py3TesterSandboxer tester(Py3InterpreterPath(), Py3LibraryPaths());
  TestOptions options;
  options.num_threads = 4;
  options.stop_on_first_failure = true;

  std::cout << R"(Trying to solve the selected problem.

There are 3 options for the outcome of the tests:
  1. The program does not compile.
  2. The program runs successfully, but gives the wrong answer sometimes.
  3. The program runs successfully and gives the correct answer in all the tests.

)";

  for (constexpr absl::string_view kSolution : sample_solutions) {
    ASSIGN_OR_RETURN(MultiTestResult result_output,
                    tester.Test(kSolution, inputs, options, outputs));
    ReportResults(result_output);
  }

  return absl::OkStatus();
}

}  // namespace
}  // namespace deepmind::code_contests

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);

  std::vector<absl::string_view> sample_solutions;
  std::ifstream sample_solutions_file("sample_solutions.json", std::ifstream::binary);

  for( std::string line; getline( sample_solutions_file, line ); ) {
    printf("%s\n", line);
  }

  // sample_solution_file >> sample_solutions;

  // REMEMBER TO SPECIFY WHICH LANGUAGE IT IS INSIDE OF EACH STRING_VIEW SOLUTION

  // NOT SURE IF NEED TO HAVE EACH kSolution as 'constexpr'? can add to for loop later?
  //constexpr absl::string_view kSolution = R"py(
  //LOAD THE ACTUAL CODE INTO HERE
  //)py";

  std::string target_problem_name = "";// DEFINE THE DESIRED PROBLEM HERE as a string;

  if (absl::Status status = deepmind::code_contests::SolveProblem(
          absl::GetFlag(FLAGS_test_path));
      !status.ok()) {
    std::cerr << "Failed: " << status.message() << std::endl;
  }
}
