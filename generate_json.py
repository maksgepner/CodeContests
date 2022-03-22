import io
import sys
import riegeli
import contest_problem_pb2
import os
import json

def _all_problems(filenames):
  """Iterates through all ContestProblems in filenames."""
  for filename in filenames:
    reader = riegeli.RecordReader(io.FileIO(filename, mode='rb'),)
    for problem in reader.read_messages(contest_problem_pb2.ContestProblem):
      yield problem

def _generate_json(filenames):
  count = 0
  f = open(os.path.join(sys.argv[-1], os.path.splitext(os.path.basename(sys.argv[1]))[0] + '.jsonl'), 'w')
  for problem in _all_problems(filenames):
    for solution in problem.solutions:
      nl = 'RATINHG: {}\nTAGS: {}\nLANGUAGE IS {}\nCORRECT SOLUTION\n{}'.\
        format(problem.cf_rating,\
          ', '.join(problem.cf_tags),\
          contest_problem_pb2.ContestProblem.Solution.Language.Name(solution.language).lower(), problem.description)
      data = json.dumps({'code': solution.solution, 'nl': nl})
      f.write(data)
      f.write('\n')
      count += 1
    for solution in problem.incorrect_solutions:
      nl = 'RATINHG: {}\nTAGS: {}\nLANGUAGE IS {}\nINCORRECT SOLUTION\n{}'.\
        format(problem.cf_rating,\
          ', '.join(problem.cf_tags),\
          contest_problem_pb2.ContestProblem.Solution.Language.Name(solution.language).lower(), problem.description)
      data = json.dumps({'code': solution.solution, 'nl': nl})
      f.write(data)
      f.write('\n')
      count += 1
  f.flush()
  f.close()
  print(f'Number of data items: {count}')

if __name__ == '__main__':
  _generate_json(sys.argv[1:-1])
