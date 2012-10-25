
NAME = runit
VERSION = 2.1.1.2012102301

SPECFILE = runit.spec
DISTFILES = doc man etc Makefile package README.md src $(SPECFILE)

sources:
	mkdir -p dist/admin/$(NAME)-$(VERSION)/
	cp -p -r -l $(DISTFILES) dist/admin/$(NAME)-$(VERSION)
	cd dist && tar czf ../$(NAME)-$(VERSION).tar.gz admin/

clean:
	rm -rf dist/ $(NAME)-$(VERSION).tar.gz
