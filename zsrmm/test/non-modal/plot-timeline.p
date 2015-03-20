
set yrange [.5:3.5]
set xtics scale 0.5, 0.25 rotate 0,1000,39000
plot './ts-task3.txt' title "Task 3" w i, './ts-task2.txt' title "Task 2" w i , './ts-task1.txt' title "Task 1" w i
