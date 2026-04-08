import os

chunk_size = 1024 * 1024
total_chunks = 1

with open("testfile1MB.bin", "wb") as f:
    for _ in range(total_chunks):
        f.write(os.urandom(chunk_size))