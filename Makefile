all : 
	make -C zsrmm
	make -C zsrmm/lib
	make -C mzstask
	make -C mzstask/sample
	make -C zsrmscheduler/edu/cmu/sei/ZeroSlackRM

clean : 
	make -C zsrmm clean
	make -C zsrmm/lib clean
	make -C mzstask clean
	make -C mzstask/sample clean
	make -C zsrmscheduler/edu/cmu/sei/ZeroSlackRM clean

