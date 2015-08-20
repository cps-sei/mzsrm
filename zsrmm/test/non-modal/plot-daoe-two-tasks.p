
set yrange [.5:3.5]
set xtics scale 0.5, 0.25 rotate 0,100,39000 font ",5"
set terminal pdf
set output 'daoe-two-task-plot.pdf'
plot './ts-task2.txt' title "Task 2" w i , './ts-task1.txt' title "Task 1" w i
