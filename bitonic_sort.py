import math
import random

'''
TODO:
Link bitonic sort to server
Add more testing sims
build map from the permutation to the table rows
Add encryption and decryption functions
Change from random module to secrets module
'''

class Server():
    def __init__(self, data):
        self.data = data
        self.access_log = []
    
    
class Client():
    def __init__(self, server, n):
        #Permutation stores the current mapping of the data
        #Cache stores recently accessed values
        self.permutation = list(range(n))
        self.server = server
        self.cache = []
        
        
    def compAndSwap(self, a, i, j, dire):
        if (dire==1 and a[i] > a[j]) or (dire==0 and a[i] < a[j]):
            a[i],a[j] = a[j],a[i]
    
    
    def bitonicMerge(self, a, low, cnt, dire):
        '''
        Recursively sorts the bitonic sequence
        If dire = 1, sorts ascending, else descending
        low is the starting index, cnt is the # of elements in the section
        '''
        if cnt > 1:
          k = cnt//2
          for i in range(low , low+k):
            self.compAndSwap(a, i, i+k, dire)
          self.bitonicMerge(a, low, k, dire)
          self.bitonicMerge(a, low+k, k, dire)
 
    def bitonicSort(self, a, low, cnt,dire):
        '''
        Produces a bitonic sequence by recursively splitting it into halves in opposite sorting orders
        Calls bitonicMerge on each part, sorting them in the determined order
        '''
        if cnt > 1:
            k = cnt//2
            self.bitonicSort(a, low, k, 1)
            self.bitonicSort(a, low+k, k, 0)
            self.bitonicMerge(a, low, cnt, dire)
    

    def sort_asc(self, N):
        random.shuffle(self.permutation)
        #assert that the input size is a power of 2
        if math.log2(N) % 1 != 0:
            raise ValueError("Input is not a power of 2")
        print(self.permutation)
        self.bitonicSort(self.permutation, 0, N, 1)
        print(self.permutation)
        
#debug
if __name__ == "__main__":
    client = Client(None)
    client.sort_asc(16)