# კომპილატორი და მისი საჭირო ოფციები;
# ამ ოფციების გარეშე gcc ვერ მიაგნებს fuse3-ის ფაილებს
CC=gcc
FLAGS=`pkg-config fuse3 --cflags --libs`

# default target რომელსაც make ასრულებს;
# მისი სინტაქსი ასეთია:
# სახელი : მოდულების სახელების რაზეც დამოკიდებულია
# 		შესასრულებელი ბრძანება
all : main.o 
	$(CC) -o cachefs main.o $(FLAGS)

# რიგითი მოდულის კონფიგურაცია:
# სახელი : დამოკიდებულებების სია (აქ შეიძლება იყოს .h ჰედერ ფაილებიც)
# 	შესასრულებელი ბრძანება
main.o : main.c 
	$(CC) -c main.c $(FLAGS)

# დაგენერირებული არტიფაქტების წაშლა
clean :
	rm cachefs main.o

# თუ პროექტს დაამატებთ .c ფაილებს, მაშინ აქ უნდა დაამატოთ ახალი მოდული, main.o-ს მსგავსად. ასევე ახალი_ფაილი.o უნდა დაუმაროთ all-ს, და clean-ს. მაგალითად:
# all : main.o new_file.o
# 	$(CC) -o cachefs main.o new_file.o $(FLAGS)
# 	
# new_file.o : new_file.c new_file.h
# 	$(CC) -c main.c $(FLAGS)
#
# clean :
# 	rm cachefs main.o new_file.o
