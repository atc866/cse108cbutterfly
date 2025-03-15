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
        
gen_data("strings17.json", 2**17)
gen_data("strings18.json", 2**18)
gen_data("strings19.json", 2**19)
gen_data("strings20.json", 2**20)
gen_data("strings21.json", 2**21)
gen_data("strings22.json", 2**22)