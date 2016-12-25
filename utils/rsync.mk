all:
	rsync --exclude=README.md -avz -e ssh user@10.1.64.188:/home/user/dev/mydir .
