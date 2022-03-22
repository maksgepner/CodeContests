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
  idx = 0
  for problem in _all_problems(filenames):
    #if idx == 0:
    print("\n\n\n====== Problem name:\n")
    print(problem.name)
    print("\nProblem description:\n")
    print(problem.description)
    idx += 1


if __name__ == '__main__':
  _print_data(sys.argv[1:])