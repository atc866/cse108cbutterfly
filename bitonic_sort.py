import math
import random

'''
TODO:
Alter input data to match testing parameters
Optimize cache size and table data structure
Alter functions to work for new table structure
Add more testing sims
Add encryption and decryption functions
Change from random module to secrets module
'''

class Server():
    def __init__(self, data):
        self.data = data
        self.access_log = []
    
    
class Client():
    #use server and table size as inputs
    def __init__(self, server):
        #Permutation stores the current mapping of the data
        #Cache stores recently accessed values
        self.permutation = {}
        self.server = server
        self.cache = []
        
        #create initial map
        for i in range(len(server.data)):
            #change server.data to whatever data index used in server
            self.permutation[i] = server.data[i]
    
    #gets a row from the server with relation to the random permutation
    def queryServer(self, index):
        row = self.server.data[self.permutation[index]]
        self.cache.append[row]
        yield row
        if len(self.cache) == 5:
            self.cache = []
            self.obvshuffle()
        
        #compares the permutations' values, if switch, reflect in 
    def compAndSwap(self, data, a, i, j, dire):
        if (dire==1 and a[i] > a[j]) or (dire==0 and a[i] < a[j]):
            a[i],a[j] = a[j],a[i]
            data[i],data[j] = data[j], data[i]
            self.permutation[i],self.permutation[j] = self.permutation[j],self.permutation[i]
    
    def bitonicMerge(self, a, data, low, cnt, dire):
        '''
        Recursively sorts the bitonic sequence
        If dire = 1, sorts ascending, else descending
        low is the starting index, cnt is the # of elements in the section
        '''
        if cnt > 1:
          k = cnt//2
          for i in range(low , low+k):
            self.compAndSwap(a, data, i, i+k, dire)
          self.bitonicMerge(a, data, low, k, dire)
          self.bitonicMerge(a, data, low+k, k, dire)
 
    def bitonicSort(self, a, data, low, cnt, dire):
        '''
        Produces a bitonic sequence by recursively splitting it and sorting it them in opposite sorting orders
        Calls bitonicMerge on each part, sorting them in the determined order
        '''
        if cnt > 1:
            #halves the # of entries for each recursion
            k = cnt//2
            self.bitonicSort(a, data, low, k, 1)
            self.bitonicSort(a, data, low+k, k, 0)
            self.bitonicMerge(a, data, low, cnt, dire)
    

    def obvshuffle(self): 
        data = self.server.data.copy()
        N = len(data)
        
        permutation = [i for i in range(N)]
        random.shuffle(permutation)
        #assert that the input size is a power of 2
        if math.log2(N) % 1 != 0:
            raise ValueError("Input is not a power of 2")
        print(self.permutation)
        print(permutation)
        print(self.server.data)
        self.bitonicSort(permutation, data, 0, N, 1)
        self.server.data = data
        print(self.permutation)
        
#debug
if __name__ == "__main__":
    n = 16
    data = [random.randint(1,100) for i in range(n)]
    server = Server(data)
    client = Client(server)
    client.obvshuffle()