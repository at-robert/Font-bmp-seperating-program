CC= gcc

file = sepFont
ALL:
	$(CC) -DsepFont=main -o $(file) $(file).c

clean:
	rm -rf $(file)

