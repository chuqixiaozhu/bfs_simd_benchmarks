#!/bin/bash

echo simd on tile 1024
./page_rank ../../datasets/pokec/soc-pokec-1024 1024
echo simd on tile 2048
./page_rank ../../datasets/pokec/soc-pokec-2048 2048
echo simd on tile 4096
./page_rank ../../datasets/pokec/soc-pokec-4096 4096
echo simd on tile 8192
./page_rank ../../datasets/pokec/soc-pokec-8192 8192
