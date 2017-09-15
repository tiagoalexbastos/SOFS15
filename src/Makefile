subdirs  = debugging
subdirs += rawIO15
subdirs += showBlock15
subdirs += sofs15
subdirs += mkfs15
subdirs += testifuncs15
subdirs += syscalls15
subdirs += mount15

.PHONY: $(subdirs)

all:	$(subdirs)

$(subdirs):
	@echo -e '\e[34;1;40;1mMaking $@...\e[m'
	@make -C $@

clean:
	@echo -e '\e[31;1;40;1mCleaning...\e[m'
	@for dir in $(subdirs) ; do make -C $$dir clean; done

