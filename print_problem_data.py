import io
import sys
import riegeli
import contest_problem_pb2

def _all_problems(filenames):
  """Iterates through all ContestProblems in filenames."""
  for filename in filenames:
    reader = riegeli.RecordReader(io.FileIO(filename, mode='rb'),)
    for problem in reader.read_messages(contest_problem_pb2.ContestProblem):
      yield problem

def _print_data(filenames):
  for problem in _all_problems(filenames):
    print("\n\n\n Problem name:\n")
    print(problem.name)
    print("\nProblem description:\n")
    print(problem.description)