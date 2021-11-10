#def fib(x):
#  if x < 2:
#    return x
#  return fib(x - 2) + fib(x - 1)
#print(fib(46))

#def fib(n):
#  if n <= 1:
#    return 1
#  return fib(n - 1) + fib(n - 2)
#print(fib(46))
#
def fib(n):
  if n <= 1: return 1
  return fib(n - 1) + fib(n - 2)

print(fib(46))