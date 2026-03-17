import os

chunk_size = 100 * 1024
total_chunks = 1

with open("testfile100KB.bin", "wb") as f:
    for _ in range(total_chunks):
        f.write(os.urandom(chunk_size))
