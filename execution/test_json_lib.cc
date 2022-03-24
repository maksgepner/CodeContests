#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>
#include "nlohmann/json.hpp"
#include <fstream>
#include <sstream>

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

// for convenience
using json = nlohmann::json;

int main() {

  // Import the .json file
  std::ifstream sample_solutions_file("/home/maksgepner/CodeGenerationAnalysis/CodeContests/execution/sample_solutions.jsonl");
	json sample_solutions;
  sample_solutions_file >> sample_solutions;

  // For getting the problem names
	std::string problem_name = sample_solutions["problem_name"].get<std::string>();
	std::cout << "\nProblem name: " << problem_name << "\n";

  // For getting the languages of the solutions
  // int i = 0;
  // for (json soln : sample_solutions["generated_solutions"]) {
  //   std::string soln_lang = soln["language"].get<std::string>();
  //   std::cout << "Solution " << i << ", language: " << soln_lang << "\n";
  //   ++i;
  // }

  // For getting the code solutions
  int i = 0;
  int i_chosen = 0;
  std::string soln_lang;
  std::string target_language = "python3";
  std::string target_correct = "correct";
  std::string soln_correct;
  absl::string_view soln_code;

  for (json soln : sample_solutions["generated_solutions"]) {
    soln_lang = soln["language"].get<std::string>();
    if (soln["is_correct"].get<bool>() == true) {
      soln_correct = "correct";
    } else {
      soln_correct = "incorrect";
    }
    // absl::string_view soln_code = soln["code"].get<absl::string_view>();
    // std::cout << "\n\n\nSolution " << i << ", code (" << soln_lang << "):\n-------------------------\n" << soln_code;
    if (soln_lang == target_language && soln_correct == "correct") {
      soln_code = soln["code"].get<absl::string_view>();
      i_chosen = i;
    }
    ++i;
  }
  std::cout << "\n\nSolution " << i_chosen << " (" << target_correct << "), code (" \
   << target_language << "):\n---------------------------------------\n" << soln_code;



  //std::string target_problem_name = "sds";// DEFINE THE DESIRED PROBLEM HERE as a string;
}