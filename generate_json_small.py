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
  f = open(os.path.join(sys.argv[-1], os.path.splitext(os.path.basename(sys.argv[1]))[0] + '.jsonl'), 'w')
  for problem in _all_problems(filenames):
    data = {'codes': [], 'rating': problem.cf_rating, 'tags': ', '.join(problem.cf_tags), 'problem': problem.description}
    for solution in problem.solutions:
      data['codes'].append({'code': solution.solution, 'language': contest_problem_pb2.ContestProblem.Solution.Language.Name(solution.language).lower(), 'is_correct': True})
    for solution in problem.incorrect_solutions:
      data['codes'].append({'code': solution.solution, 'language': contest_problem_pb2.ContestProblem.Solution.Language.Name(solution.language).lower(), 'is_correct': False})
    f.write(json.dumps(data))
    f.write('\n')
  f.flush()
  f.close()

if __name__ == '__main__':
  _generate_json(sys.argv[1:-1])
