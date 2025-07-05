# min-dominating-set

This repository implements the dynamic programming on graph tree-decompositions algorithm described in `Parameterized Algorithms` to compute the size of the minimal dominating set.</br>
The goal of this algorithm is to achieve runtime bounded exponentially only in the treewidth of the given tree-decomposition, instead of the size of the vertex-set of the original graph.</br>
The algorithm is implemented in C++, which calls a python script to read input data and construct a nice-tree-decomposition with the help of sage.</br>
I implemented 2 versions: 
1. `decomp.cpp` makes use of c++ `std::unordered_map` (HashMap). This enables fast lookup of the value of partial solutions during the tree-decomposition traversal but increases the time needed to set up the data-structures substantially.  
2. `decompNoHash.cpp` stores values of partial solutions in vectors. 

## Compilation and Usage
The implemented programs make use of `sage` to compute graphs, tree-decompositions, nice-tree-decompositions and check validity of given tree-decompositions, from the input data. Before you can run the programs you need to make sure that your environment is set up with sage.</br>
Run `g++ -O3 decomp.cpp -o decomp` or `g++ -O3 decompNoHash.cpp -o decompNoHash` respectively to compile.</br>
Then you can run `./decomp file.gr` to let sage create a nice-TD of the graph described in `file.gr` and then run the algorithm on it.</br>
You can also rin `./decomp file.gr file.td` to skip the expensive TD creation, if you already have a TD of the `.gr` file saved in the `.td` file. The python script will check, that it is indeed a valid TD of this graph.</br>
(You can also just run the python-script as a standalone to cerate TDs, or output data about the created structures which are then processed by the algorithm.)
