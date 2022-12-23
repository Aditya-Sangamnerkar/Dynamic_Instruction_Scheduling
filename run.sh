make clean
g++ sim_proc.cpp -o sim_proc.o
make
./sim  512 64 7 val_trace_perl1 > output.txt
diff -iw output.txt "E:\NCSU\fall2022\ECE563\project3\validation\val8.txt" > difference.txt