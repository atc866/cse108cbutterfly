import math
import random
import json
from cryptography.fernet import Fernet

# Simulated untrusted memory storage.
untrusted_memory = {}

class Enclave:
    def __init__(self):
        # Generate a symmetric encryption key and initialize a Fernet instance.
        self.key = Fernet.generate_key()
        self.fernet = Fernet(self.key)
    
    def ocall_write_bucket(self, level, bucket_index, bucket):
        # Encrypt the bucket and write to untrusted memory.
        encrypted_bucket = self.encrypt_bucket(bucket)
        untrusted_memory[(level, bucket_index)] = encrypted_bucket
        print(f"OCALL WRITE: Level {level}, Bucket {bucket_index}")
    
    def ocall_read_bucket(self, level, bucket_index):
        # Read the encrypted bucket from untrusted memory and decrypt it.
        encrypted_bucket = untrusted_memory.get((level, bucket_index))
        if encrypted_bucket is None:
            return None
        bucket = self.decrypt_bucket(encrypted_bucket)
        print(f"OCALL READ: Level {level}, Bucket {bucket_index}")
        return bucket
    
    def encrypt_bucket(self, bucket):
        # Serialize the bucket as JSON, encode as bytes, and encrypt with Fernet.
        data = json.dumps(bucket).encode('utf-8')
        return self.fernet.encrypt(data)
    
    def decrypt_bucket(self, data):
        # Decrypt the data and deserialize the JSON.
        decrypted = self.fernet.decrypt(data)
        return json.loads(decrypted.decode('utf-8'))
    
    def oblivious_sort(self, input_array, bucket_size):
        """
        Performs bucket oblivious sort in the enclave.
        input_array: list of values to be sorted.
        bucket_size (Z): fixed size for each bucket; each bucket can hold at most Z/2 real elements.
        """
        n = len(input_array)
        Z = bucket_size
        
        # Compute B = smallest power of two >= ceil(2*n / Z)
        B_required = math.ceil(2 * n / Z)
        B = 1
        while B < B_required:
            B *= 2
        L = int(math.log2(B))  # Number of levels
        
        if n > B * (Z // 2):
            raise ValueError("Bucket size too small for input size.")
        
        # --- Step 1: Assign random keys and create initial buckets (level 0) ---
        # Each element is paired with a random key in [0, B-1].
        elements = [(x, random.randint(0, B - 1)) for x in input_array]
        group_size = math.ceil(n / B)
        groups = [elements[i * group_size: (i + 1) * group_size] for i in range(B)]
        
        # Each bucket is padded with dummy elements (None) to have exactly Z entries.
        for i in range(B):
            bucket = groups[i]
            padded_bucket = bucket + [None] * (Z - len(bucket))
            self.ocall_write_bucket(0, i, padded_bucket)
        
        # --- Step 2: Oblivious Random Bin Assignment via Butterfly Network ---
        for level in range(0, L):
            for i in range(0, B, 2):
                bucket1 = self.ocall_read_bucket(level, i)
                bucket2 = self.ocall_read_bucket(level, i + 1)
                out_bucket0, out_bucket1 = self.merge_split(bucket1, bucket2, level, L, Z)
                self.ocall_write_bucket(level + 1, i, out_bucket0)
                self.ocall_write_bucket(level + 1, i + 1, out_bucket1)
        
        # --- Step 3: Final Stage (Oblivious Random Permutation) ---
        final_elements = []
        for i in range(B):
            bucket = self.ocall_read_bucket(L, i)
            # Remove dummy elements.
            real_elements = [elem for elem in bucket if elem is not None]
            # Shuffle locally to simulate oblivious permutation within the bucket.
            random.shuffle(real_elements)
            final_elements.extend(real_elements)
        
        # --- Step 4: Non-oblivious Sort ---
        sorted_elements = sorted(final_elements, key=lambda tup: tup[0])
        # Return only the sorted values.
        return [val for val, _ in sorted_elements]
    
    def merge_split(self, bucket1, bucket2, level, total_levels, Z):
        """
        MergeSplit subroutine that combines two buckets and partitions real elements into two output buckets.
        The partitioning is based on the (level+1)-th most significant bit of each element's key.
        """
        L = total_levels
        bit_index = L - 1 - level  # For level 0, use the MSB; for level 1, the next bit, etc.
        combined = bucket1 + bucket2
        real_elements = [elem for elem in combined if elem is not None]
        
        out_bucket0 = []
        out_bucket1 = []
        for value, key in real_elements:
            if ((key >> bit_index) & 1) == 0:
                out_bucket0.append((value, key))
            else:
                out_bucket1.append((value, key))
        
        if len(out_bucket0) > Z or len(out_bucket1) > Z:
            raise OverflowError("Bucket overflow in merge_split.")
        
        # Pad the buckets with dummies.
        out_bucket0 += [None] * (Z - len(out_bucket0))
        out_bucket1 += [None] * (Z - len(out_bucket1))
        return out_bucket0, out_bucket1

# === Example Usage ===
if __name__ == "__main__":
    input_data = [random.randint(100, 200) for _ in range(20)]
    print("Input Data:", input_data)
    
    enclave = Enclave()
    sorted_data = enclave.oblivious_sort(input_data, bucket_size=8)
    print("Sorted Data:", sorted_data)
