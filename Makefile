# set shell to make sure that redirection works (dash doesn't seem to have it)
SHELL=/bin/bash

TITLE = README
SPELL_CHECK ?= $(TITLE).md
PDFVIEWER ?= okular

all: $(TITLE).pdf

$(TITLE).pdf: $(TITLE).md header.md
	cat header.md $(TITLE).md | pandoc -o $(TITLE).pdf

view:
	$(PDFVIEWER) $(TITLE).pdf &>/dev/null &

spell:
	aspell --lang=en_US check $(SPELL_CHECK)
