calls = 0
def fib(x):
  global calls
  calls += 1
  if x < 2:
    return x
  return fib(x - 2) + fib(x - 1)
print(fib(30))
print('calls ', calls)