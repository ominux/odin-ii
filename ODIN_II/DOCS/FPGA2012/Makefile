FILE = vtrpaper
TEX_FILES = vtrpaper.tex references.bib structure.tex paper_acronym.tex abstract.tex intro.tex overview.tex odin2.tex abc.tex vpr.tex benchmarks.tex exampleuse.tex futurework.tex conclusions.tex

TEX = TEXINPUTS=.:/jayar/r/r1/ikuon/tex:/Home/ian/tex/:;export TEXINPUTS; latex

PDFTEX = pdflatex

BIBTEX = bibtex

DVI2PS = dvips

PS2PDF = ps2pdf
DISTILL = distill

LPRPS = lp -o nobanner

# The following assumes the existence of two
# environment variables: PRINTER1 and PRINTER2,
# where typically PRINTER2 may be double-sided.
#
pr1 :=  PRINTER = $(PRINTER1)
pr2 :=  PRINTER = $(PRINTER2)

RM = /bin/rm -f

SPELL = Spell.ok
TMP = /tmp/$(USER).JUNK

all : $(FILE).ps
	- $(PS2PDF) $(FILE).ps $(FILE).pdf

$(FILE).dvi : $(FILE).tex ${TEX_FILES}
	$(TEX) $(FILE)
	 $(BIBTEX) $(FILE)
	 $(TEX) $(FILE)
	 $(TEX) $(FILE)
	 $(TEX) $(FILE)


ps $(FILE).ps : $(FILE).dvi
	- $(DVI2PS) -t letter -Ppdf -G0 -o $(FILE).ps $(FILE)

pdf $(FILE).pdf : $(FILE).ps
	- $(PS2PDF) $(FILE).ps $(FILE).pdf

pdftex $(FILE)_1.pdf : $(FILE).tex ${TEX_FILES}
	$(PDFTEX) $(FILE)
	$(BIBTEX) $(FILE)
	- $(PDFTEX) $(FILE)
	- $(PDFTEX) $(FILE)
	- $(PDFTEX) $(FILE)

final : $(FILE).ps
	gs -dSAFER -dNOPAUSE -dBATCH -sDEVICE=pdfwrite -sPAPERSIZE=letter -dPDFSETTINGS=/printer -dCompatibilityLevel=1.3 -dMaxSubsetPct=100 -dSubsetFonts=true -dEmbedAllFonts=true -sOutputFile=$(FILE).pdf $(FILE).ps

sp:     $(FILE).tex
	- cat $(FILE).tex | spell +$(SPELL)

pr pr1 pr2 : $(FILE).ps
	- $(LPRPS) $(FILE).ps

clean :
	$(RM) $(FILE).ps $(FILE).dvi $(FILE).pdf $(FILE).aux $(FILE).blg $(FILE).bbl *.tex~ $(FILE).out

sort:   $(SPELL)
	sort < $(SPELL) > $(TMP)
	mv $(TMP) $(SPELL)

