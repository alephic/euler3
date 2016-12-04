
from random import random

n = 7
tmax = 20
ps = [random()*20 - 10 for i in range(n*3)]
for t in range(tmax):
  for i in range(len(ps)):
    if i > 0:
      print(' ', end='')
    print(ps[i], end='')
    ps[i] += random() - 0.5
  print()


