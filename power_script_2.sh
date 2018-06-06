#!/bin/bash

#./src/booksim ./sims/mycmesh >./sims/cmesh/first.txt &

# UNIFORM TRAFFIC 
./src/booksim ./sims/mykncube injection_rate=.1 traffic=uniform > ./sims/kncube/uniform_1.txt &
./src/booksim ./sims/mykncube injection_rate=.2 traffic=uniform > ./sims/kncube/uniform_2.txt &
./src/booksim ./sims/mykncube injection_rate=.3 traffic=uniform > ./sims/kncube/uniform_3.txt &
./src/booksim ./sims/mykncube injection_rate=.4 traffic=uniform > ./sims/kncube/uniform_4.txt &

wait
echo ==========================================================================
echo NEXT SIM
echo ==========================================================================
# ./src/booksim ./sims/
wait
echo done!