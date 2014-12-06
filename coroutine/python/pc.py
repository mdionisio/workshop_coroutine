def producer():
  n = 0
  while True:
    yield n
    n += 1
    
def consumer():
  for data in producer():
    print data
    
    
consumer()