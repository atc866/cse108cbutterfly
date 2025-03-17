import random
import math
import string

def gen_data(name, count):
    file = open(name, "w")
    file.write("[")
    for i in range(count):
        random_string = ''.join(random.choices(string.ascii_letters + string.digits, k=random.randrange(1,20)))
        random_string.replace(",", "")
        file.write(random_string)
        file.write(",")
    file.write("]")
    file.close()
    
def gen_bucket_data(name, count, size):
    indexes = list(range(0, count))
    file = open(name, "w")
    file.write("[\n")
    for i in range(count):
        random_string = ''.join(random.choices(string.ascii_letters + string.digits, k=size))
        random_index = random.randint(0, count)
        random_string.replace(",", "")
        file.write('  {"sort_value": %d, "payload": "%s"},\n' %(random_index, random_string))
    file.write("]")
    file.close()
        
gen_bucket_data("bstrings22.json", 2**22, 4)
