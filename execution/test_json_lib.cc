//#include <functional>
//#include <iostream>
//#include <string>
//#include <tuple>
//#include <vector>
#include "nlohmann/json.hpp"
// for convenience
#include <fstream>

using json = nlohmann::json;

int main() {

  std::ifstream sample_solutions_file("sample_solutions.jsonl");
	json sample_solutions;
  sample_solutions_file >> sample_solutions;

	// json soln = sample_solutions["problem_name"].get<std::string>();
	// std::cout << soln;
	std::cout << "Printing correctly.";
	std::cout << sample_solutions;

  //for( std::string line; getline( sample_solutions_file, line ); ) {
    //std::cout << line;
    //printf("%s\n", line.c_str());
  //}

  // sample_solution_file >> sample_solutions;

  // REMEMBER TO SPECIFY WHICH LANGUAGE IT IS INSIDE OF EACH STRING_VIEW SOLUTION

  // NOT SURE IF NEED TO HAVE EACH kSolution as 'constexpr'? can add to for loop later?
  //constexpr absl::string_view kSolution = R"py(
  //LOAD THE ACTUAL CODE INTO HERE
  //)py";

  //std::string target_problem_name = "sds";// DEFINE THE DESIRED PROBLEM HERE as a string;

}