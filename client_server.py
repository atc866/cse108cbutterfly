import math
import random

class Server:
    def __init__(self):
        # Storage: keys are (level, bucket_index) and values are lists representing buckets.
        self.storage = {}
        self.access_log = []  # Log each access for simulation purposes

    def read_bucket(self, level, bucket_index):
        self.access_log.append(f"Read bucket at level {level}, index {bucket_index}, {self.storage[(level,bucket_index)]}")
        return self.storage.get((level, bucket_index), None)

    def write_bucket(self, level, bucket_index, bucket):
        self.access_log.append(f"Write bucket at level {level}, index {bucket_index}, {bucket}")
        self.storage[(level, bucket_index)] = bucket

    def get_access_log(self):
        return self.access_log


class Client:
    def __init__(self, server):
        self.server = server

    def oblivious_sort(self, input_array, bucket_size):
        """
        Performs an oblivious sort on input_array using the bucket oblivious sort technique.
        bucket_size (Z) must be chosen so that each bucket can hold at most Z/2 real elements.
        """
        n = len(input_array)
        Z = bucket_size
        
        # Compute B = smallest power of two >= ceil(2*n / Z)
        B_required = math.ceil(2 * n / Z)
        B = 1
        while B < B_required:
            B *= 2
        L = int(math.log2(B))  # Number of levels in the butterfly network

        # Ensure that each bucket (of capacity Z) can store at most Z/2 real elements.
        if n > B * (Z // 2):
            raise ValueError("Bucket size too small for input size.")

        # --- Step 1: Assign random keys and create initial buckets (level 0) ---
        # Each element becomes a tuple (value, key), where key ∈ [0, B-1]
        elements = [(x, random.randint(0, B - 1)) for x in input_array]
        
        # Partition the elements into B groups.
        group_size = math.ceil(n / B)
        groups = [elements[i * group_size: (i + 1) * group_size] for i in range(B)]
        
        # Each bucket is padded with dummies (None) to have exactly Z entries.
        for i in range(B):
            bucket = groups[i]
            padded_bucket = bucket + [None] * (Z - len(bucket))
            self.server.write_bucket(0, i, padded_bucket)

        # --- Step 2: Oblivious Random Bin Assignment via Butterfly Network ---
        # For each level from 0 to L-1, pair buckets and call merge_split.
        for level in range(L):
            for i in range(0, B, 2):
                bucket1 = self.server.read_bucket(level, i)
                bucket2 = self.server.read_bucket(level, i + 1)
                out_bucket0, out_bucket1 = self.merge_split(bucket1, bucket2, level, L, Z)
                # Write the resulting buckets to the next level at the same indices.
                self.server.write_bucket(level + 1, i, out_bucket0)
                self.server.write_bucket(level + 1, i + 1, out_bucket1)

        # --- Step 3: Final Stage (Oblivious Random Permutation) ---
        # probably need better random permutation
        final_elements = []
        for i in range(B):
            bucket = self.server.read_bucket(L, i)
            # Remove dummy elements (None)
            real_elements = [elem for elem in bucket if elem is not None]
            # Locally shuffle the real elements (simulate oblivious permutation within a bucket)
            random.shuffle(real_elements)
            final_elements.extend(real_elements)
        
        # final_elements now contains a random permutation (ORP) of the input elements.
        # --- Step 4: Non-oblivious Sort to achieve overall oblivious sort ---
        # Sorting the ORP output (by the original value) gives the sorted array.
        sorted_elements = sorted(final_elements, key=lambda tup: tup[0])
        # Return only the values (discarding the random keys)
        return [val for val, _ in sorted_elements]

    def merge_split(self, bucket1, bucket2, level, total_levels, Z):
        """
        Simulates the MergeSplit operation.
        Combines two buckets and partitions the real elements into two output buckets
        according to the (level+1)-th most significant bit of their key.
        The bit used is: bit_index = (total_levels - 1 - level)
        """
        L = total_levels
        bit_index = L - 1 - level  # For level 0, use the MSB; for level 1, the next bit, etc.
        
        # Combine the two buckets.
        combined = bucket1 + bucket2
        # Filter out dummy elements.
        real_elements = [elem for elem in combined if elem is not None]
        
        # Partition real elements based on the designated bit of the key.
        out_bucket0 = []
        out_bucket1 = []
        for value, key in real_elements:
            if ((key >> bit_index) & 1) == 0:
                out_bucket0.append((value, key))
            else:
                out_bucket1.append((value, key))
        
        # In a complete implementation we would check for overflow.
        if len(out_bucket0) > Z or len(out_bucket1) > Z:
            raise OverflowError("Bucket overflow occurred in merge_split.")
        
        # Pad buckets with dummy elements (None) so that each bucket has exactly Z entries.
        out_bucket0 += [None] * (Z - len(out_bucket0))
        out_bucket1 += [None] * (Z - len(out_bucket1))
        return out_bucket0, out_bucket1


# === Example Usage ===
if __name__ == "__main__":
    # Create a random list of integers to sort.
    input_data = [random.randint(1, 100) for _ in range(10)]
    print("Input Data:", input_data)
    
    # Instantiate the server and client.
    server = Server()
    client = Client(server)
    
    # Use a bucket size (Z) that is large enough so that each bucket holds at most Z/2 real elements.
    sorted_data = client.oblivious_sort(input_data, bucket_size=8)
    print("Sorted Data:", sorted_data)
    
    # Optionally, print the access log to see the simulated memory accesses.
    print("\nAccess Log:")
    for log in server.get_access_log():
        print(log)
