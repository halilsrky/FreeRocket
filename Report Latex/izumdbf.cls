\NeedsTeXFormat{LaTeX2e}[1996/12/01]
\LoadClass[a4paper,oneside,onecolumn,12pt]{report}[1996/10/31]
\RequirePackage{ifthen}
\RequirePackage[utf8x]{inputenc}
\RequirePackage[T1]{fontenc}
\RequirePackage{fancyhdr}
\RequirePackage[hang,labelsep=period]{caption}
\RequirePackage{calc}
\captionsetup [table]{position=top, aboveskip=5pt, belowskip=5pt}
\captionsetup [figure]{position=bottom, aboveskip=5pt, belowskip=5pt}

\DeclareOption{openbib}{\PassOptionsToClass{openbib}{report}}
\DeclareOption{draft}{\PassOptionsToClass{draft}{report}}
\DeclareOption{final}{\PassOptionsToClass{final}{report}}
\DeclareOption{openright}{\PassOptionsToClass{openright}{report}}
\DeclareOption{openany}{\PassOptionsToClass{openany}{report}}

\ProcessOptions

\lhead{}
\chead{}
\rhead{\thepage}
\lfoot{}
\cfoot{}
\rfoot{}
\renewcommand{\headrulewidth}{0pt}

\hyphenpenalty=1000
\exhyphenpenalty=10000

\newlength{\ChapterUstenBosluk}
\setlength{\ChapterUstenBosluk}{30mm}

\newlength{\mistixI}
\newlength{\mistixII}

\newlength{\LoFparskip}     %% List of Figure parskip 08-05-2006
\setlength{\LoFparskip}{0em}

\newlength{\LoTparskip}     %% List of Table parskip 08-05-2006
\setlength{\LoTparskip}{0em}

\newlength{\ToCparskip}     %% Table of Content parskip 08-05-2006
\setlength{\ToCparskip}{0em}

\newif\if@AppendixVar  %% Ekler Olsun mu?
\@AppendixVarfalse

\newif\if@SembolListesiVar  
\@SembolListesiVarfalse

\newif\if@KisaltmalarVar 
\@KisaltmalarVarfalse

\newif\if@TabloListesiVar   %% Tablo Listesi 
\@TabloListesiVartrue       %% Default = Var

\newif\if@SekilListesiVar   %% Sekil Listesi 
\@SekilListesiVartrue       %% Default = Var

\newif\if@ToCisDotted   %% ToC Noktalı / Noktasız
\@ToCisDottedfalse      %% Default = Noktasız

\newif\if@ChapterBoldToC %% ToC da BÖLÜM 1-2.. 
\@ChapterBoldToCtrue    %% Default = Bold Değil


\newif\if@TesekkurVar   %% Teşekkür var mı?
\@TesekkurVarfalse      %% Default = False

\newif\if@JuriVar       %% Juri isimleri Girilmiş mi?
\@JuriVarfalse          %% Default = False

\newif\if@AuthorIkiVar       %% Yazar 2 isimleri Girilmiş mi?
\@AuthorIkiVarfalse          %% Default = False

\newif\if@AuthorUcVar       %% Yazar 3 isimleri Girilmiş mi?
\@AuthorUcVarfalse          %% Default = False

\newif\if@AuthorDortVar       %% Yazar 4 isimleri Girilmiş mi?
\@AuthorDortVarfalse          %% Default = False

\newif\if@AuthorBesVar       %% Yazar 5 isimleri Girilmiş mi?
\@AuthorUcVarfalse         


\newcommand*{\TabloListesiYok}{\@TabloListesiVarfalse}

\newcommand*{\SekilListesiYok}{\@SekilListesiVarfalse}

\newcommand*{\ToCLineisDotted}{\@ToCisDottedtrue}

\newcommand*{\ToCChapterBold}{\@ChapterBoldToCtrue}

\newcommand{\th@StoreIn}[2]{\gdef#1{#2}}  %% Stores the second arg in the command specified by the first arg.

\newcommand*{\degree}[3]
    {
        \th@StoreIn{\th@fulldegree}{#1}
        \th@StoreIn{\th@degree}{#2}
        \th@StoreIn{\th@longdegree}{#3}
    }
    
\newcommand*{\turkdegree}[2]
    {
        \th@StoreIn{\th@turkfulldegree}{#1}
        \th@StoreIn{\th@turkdegree}{#2}
    }

\renewcommand*{\author}[5]{
    \th@StoreIn{\th@isimsoyisim}{#1 #2}
    \th@StoreIn{\th@soyisimisim}{#2, #1}
    \th@StoreIn{\th@numaraBir}{#3}
    \th@StoreIn{\th@mailBir}{#4}
    \th@StoreIn{\th@bolumBir}{#5}
}

\newcommand*{\authorIki}[5]{
	\@AuthorIkiVartrue
	\th@StoreIn{\th@isimsoyisimIki}{#1 #2}
	\th@StoreIn{\th@soyisimisimIki}{#2, #1}
	  \th@StoreIn{\th@numaraIki}{#3}
	  \th@StoreIn{\th@mailIki}{#4}
	  \th@StoreIn{\th@bolumIki}{#5}
}

\newcommand*{\authorUc}[5]{
	\@AuthorUcVartrue
	\th@StoreIn{\th@isimsoyisimUc}{#1 #2}
	\th@StoreIn{\th@soyisimisimUc}{#2, #1}
	  \th@StoreIn{\th@numaraUc}{#3}
	  \th@StoreIn{\th@mailUc}{#4}
	  \th@StoreIn{\th@bolumUc}{#5}
}

\newcommand*{\authorDort}[5]{
	\@AuthorDortVartrue
	\th@StoreIn{\th@isimsoyisimDort}{#1 #2}
	\th@StoreIn{\th@soyisimisimDort}{#2, #1}
	  \th@StoreIn{\th@numaraDort}{#3}
	  \th@StoreIn{\th@mailDort}{#4}
	  \th@StoreIn{\th@bolumDort}{#5}
}

\newcommand*{\authorBes}[5]{
    \@AuthorDortVartrue
    \th@StoreIn{\th@isimsoyisimBes}{#1 #2}
    \th@StoreIn{\th@soyisimisimBes}{#2, #1}
      \th@StoreIn{\th@numaraBes}{#3}
      \th@StoreIn{\th@mailBes}{#4}
      \th@StoreIn{\th@bolumBes}{#5}
}

\newcommand*{\juri}[2]{
  \@JuriVartrue
  \th@StoreIn{\th@juribir}{#1}
  \th@StoreIn{\th@juriiki}{#2}
}
\newcommand{\Yil}[1]{\th@StoreIn{\th@Yil}{#1}}
\newcommand{\Donem}[1]{\th@StoreIn{\th@Donem}{#1}}
\newcommand{\raporNo}[1]{\th@StoreIn{\th@raporNo}{#1}}

\newcommand{\teziyoneten}[1]{\th@StoreIn{\th@TeziYoneten}{#1}}
\newcommand{\teziyonetenenglish}[1]{\th@StoreIn{\th@TeziYonetenenglish}{#1}}
\newcommand{\anabilimdali}[1]{\th@StoreIn{\th@AnaBilimDali}{#1}}
\newcommand{\anabilimdaliUpper}[1]{\th@StoreIn{\th@AnaBilimDaliUpper}{#1}}


\newcommand*{\th@titleFtSz}{\fontseries{b}\fontsize{16}{16}\selectfont}             %% Title Font Boyutu
\newcommand*{\th@ToCTitleFtSz}{\fontfamily{cmr}\fontseries{b}\selectfont}                           %% Icindekiler Sayfasindaki Title Font Boyutu
\newcommand*{\th@titleAnaBilimDaliFtSz}{\fontseries{b}\fontsize{16}{16}\selectfont}
\newcommand*{\th@DestekFtSz}{\fontseries{b}\fontsize{14}{14}\selectfont}            %% Destek Font Boyutu
\newcommand*{\th@HdFnt}{\fontfamily{cmr}\selectfont\bfseries}                       %% Başlıklar Kalın (Bold) olacak
\newcommand*{\th@ChSz}{\normalsize\fontsize{12}{12}\selectfont}                     %% Chapter Font Boyutu 
\newcommand*{\th@ScSz}{\normalsize\fontsize{12}{12}\selectfont}                     %% Section Font Boyutu

\renewcommand{\title}[1]{\th@StoreIn{\th@title}{#1}}
\newcommand{\titleenglish}[1]{\th@StoreIn{\th@titleenglish}{#1}}
\newcommand{\titleCase}[1]{\th@StoreIn{\th@titleCase}{#1}}
\renewcommand{\date}[1]{\th@StoreIn{\th@date}{#1}}
\newcommand{\dateenglish}[1]{\th@StoreIn{\th@dateenglish}{#1}}

%
\newlength{\defbaselineskip}
\setlength{\defbaselineskip}{\baselineskip}
\newcommand{\setlinespacing}[1]{\setlength{\baselineskip}{#1 \defbaselineskip}}       
\newcommand{\oneandonehalf}{\setlength{\baselineskip}{1.5 \defbaselineskip}}            %%\oneandonehalf = 1.5*\defbaselineskip

\newcommand{\LoFspacing}{\setlength{\baselineskip}{1.3 \defbaselineskip}}               %%\LoFspacing = 1.2*\defbaselineskip 08-05-2006
\newcommand{\LoTspacing}{\setlength{\baselineskip}{1.3 \defbaselineskip}}               %%\LoTspacing = 1.2*\defbaselineskip 08-05-2006
\newcommand{\ToCspacing}{\setlength{\baselineskip}{1.3 \defbaselineskip}}               %%\ToCspacing = 1.2*\defbaselineskip 08-05-2006

\newlength{\spacebeforechapterToC}
\setlength{\spacebeforechapterToC}{10pt}

\newlength{\spaceafterchapterToC} 
\setlength{\spaceafterchapterToC}{2pt}

\AtBeginDocument{
        \setlength{\voffset}{-5.4mm}
        \setlength{\hoffset}{14.6mm}
        \setlength{\oddsidemargin}{0mm}      %% Sayfa Solu = 1in(25.4 mm) + 14.6mm = 40 mm
        \setlength{\evensidemargin}{0mm}     %% Sayfa Sağı = 1in(25.4 mm) + 14.6mm = 40 mm
        \setlength{\topmargin}{0mm}          %% Sayfa Üstü = 1in(25.4) -5.4mm = 20 mm --> Sayfa Numarasına (Sayfa Başlığına) Kadar Olan Kısım
        \setlength{\headheight}{10.0mm}         %% Sayfa Başlığı (Numarası) = 10 mm
        \setlength{\headsep}{5.0mm}             %% Sayfa Başlığı ile Sayfa Metni Arası Boşluk
        \setlength{\textheight}{242mm}          %% Sayfa Metni Yüksekliği
        \setlength{\textwidth}{150mm}           %% Sayfa Metni Genişliği
        \setlength{\marginparsep}{0mm}
        \setlength{\marginparwidth}{0mm}
        \setlength{\leftmargini}{1.25em}
        \setlength{\leftmarginii}{2.2em}
        \setlength{\leftmarginiii}{2.2em}
        \setlength{\leftmarginiv}{2.2em}
        \setlength{\leftmarginv}{2.2em}
        \setlength{\leftmarginvi}{2.2em}
        \setlength{\leftmargin}{0mm}
        \setlength{\labelsep}{.5em}
        \setlength{\labelwidth}{1.5em}
        \setlength{\footskip}{0mm}
        \setlength{\topskip}{0mm}
        \setlength{\parindent}{0pt}             %% Paragrafların ilk satırlarının ne kadar içeri gireceği
        \setlength\parskip{10pt}
        \pagestyle{fancy}
        \markright{}
        \onecolumn
        \raggedbottom
        \normalsize
        \normalfont
        \oneandonehalf
        \setcounter{tocdepth}{5}
        \setcounter{secnumdepth}{10}            %% Bütün başlıklar numaralanacak
        }
        
\AtEndDocument{

}

\newcommand*{\th@ChapterStyle}{\th@HdFnt\th@ChSz}
\newcommand*{\th@SectionStyle}{\th@HdFnt\th@ScSz}
\newcommand*{\th@SubSectionStyle}{\th@HdFnt\th@ScSz}
\newcommand*{\th@OtherSecHeadStyles}{\th@HdFnt\th@ScSz}


\newcommand*{\th@DoChapCenter}{\centering}
\newcommand*{\th@ChapHead}[1]{\protect\th@ChapterStyle \protect\th@DoChapCenter #1}

\newcommand*{\chapternameToC}{\fontfamily{cmr}\normalsize\selectfont BÖLÜM\normalfont}
\newcommand*{\contentsnameToC}{\fontfamily{cmr}\normalsize\selectfont İÇİNDEKİLER\normalfont}
\newcommand*{\listtablenameToC}{\fontfamily{cmr}\normalsize\selectfont TABLOLAR LİSTESİ\normalfont}
\newcommand*{\listfigurenameToC}{\fontfamily{cmr}\normalsize\selectfont ŞEKİLLER LİSTESİ\normalfont}
\newcommand*{\abstractnameToC}{\fontfamily{cmr}\normalsize\selectfont ABSTRACT\normalfont}
\newcommand*{\ozetnameToC}{\fontfamily{cmr}\normalsize\selectfont ÖZET\normalfont}
\newcommand*{\tesekkurnameToC}{\fontfamily{cmr}\normalsize\selectfont  ÖNSÖZ / TEŞEKKÜR\normalfont}
\newcommand*{\girisnameToC}{\fontfamily{cmr}\normalsize\selectfont GİRİŞ\normalfont}
\newcommand*{\ozgecmisnameToC}{\fontfamily{cmr}\normalsize\selectfont ÖZGEÇMİŞ\normalfont}
\newcommand*{\symbolnameToC}{\fontfamily{cmr}\normalsize\selectfont SEMBOLLER LİSTESİ\normalfont}
\newcommand*{\abbreviationnameToC}{\fontfamily{cmr}\normalsize\selectfont KISALTMALAR\normalfont}
\newcommand*{\appendixnameToC}{\fontfamily{cmr}\normalsize\selectfont EKLER\normalfont}
\newcommand*{\bibnameToC}{\fontfamily{cmr}\normalsize\selectfont KAYNAKÇA\normalfont}
\newcommand*{\preappendixname}{EK}

%\def\prefacename{\th@ChapHead{ÖNSÖZ / TEŞEKKÜR}}%
\def\refname{\th@ChapHead{KAYNAKÇA}}%
\def\abstractname{\th@ChapHead{ÖZET}}%
\def\bibname{\th@ChapHead{KAYNAKÇA}}%
\def\chaptername{\th@ChapHead{BÖLÜM}}%
\def\appendixname{\th@ChapHead{EK}}%
\def\contentsname{\th@ChapHead{İÇİNDEKİLER}}%
\def\listfigurename{\th@ChapHead{ŞEKİLLER LİSTESİ}}%
\def\listtablename{\th@ChapHead{TABLOLAR LİSTESİ}}%
\def\indexname{\th@ChapHead{DİZİN}}%
\def\figurename{Şekil}%
\def\tablename{Tablo}%
\def\partname{\th@ChapHead{KISIM}}%
\def\glossaryname{\th@ChapHead{TERİMLER SÖZLÜĞÜ}}
\def\symbolname{\th@ChapHead{SEMBOLLER LİSTESİ}}
\def\abbreviationname{\th@ChapHead{KISALTMALAR}}

\newcommand*{\th@DefTheChapter}{\arabic{chapter}}
\newcommand*{\th@DefTheSection}{\arabic{section}}
\newcommand*{\th@DefTheSubsection}{\th@DefTheSection.\arabic{subsection}}
\newcommand*{\th@DefTheSubsubsection}{\th@DefTheSubsection.\arabic{subsubsection}}
\newcommand*{\th@DefTheParagraph}{\th@DefTheSubsubsection.\arabic{paragraph}}

\renewcommand*{\thechapter}{\th@DefTheChapter}
\renewcommand*{\thesection}{\th@DefTheSection}
\renewcommand*{\thesubsection}{\th@DefTheSubsection}
\renewcommand*{\thesubsubsection}{\th@DefTheSubsubsection}
\renewcommand*{\theparagraph}{\th@DefTheParagraph}

\renewcommand*\l@chapter[2]{%
  \ifnum \c@tocdepth >\m@ne
    \addpenalty{-\@highpenalty}%
    \vskip 1.0em \@plus\p@
    \setlength\@tempdima{1.5em}%
    \begingroup
      \parindent \z@ \rightskip \@pnumwidth
      \parfillskip -\@pnumwidth
      \leavevmode \bfseries
      #1 \nobreak
       \if@ToCisDotted
       \leaders\hbox{$\m@th
        \mkern \@dotsep mu\hbox{.}\mkern \@dotsep
         mu$}
       \fi
      \hfil\nobreak\hb@xt@\@pnumwidth{\hss #2}\par
      \penalty\@highpenalty
    \endgroup
  \fi}
  
\def\@tocline#1#2#3#4#5{%
  \vskip 10\p@
  \ifnum #1>\c@tocdepth \else
    \vskip \z@ \@plus.2\p@
    {\leftskip #2\relax \rightskip \@tocrmarg \parfillskip -\rightskip
     \parindent #2\relax\@afterindenttrue
     \interlinepenalty\@M
     \leavevmode
     \@tempdima #3\relax
     \advance\leftskip \@tempdima \null\nobreak\hskip -\leftskip
     {#4}\nobreak
     \if@ToCisDotted
     \leaders\hbox{$\m@th
        \mkern \@dotsep mu\hbox{.}\mkern \@dotsep
          mu$}
     \fi
     \hfill
     \nobreak
     \nobreak\hb@xt@\@pnumwidth{\hss\protect\bf#5}
     \par}%
  \fi}

\def\@toclinenonbold#1#2#3#4#5{%
  \vskip 10\p@
  \ifnum #1>\c@tocdepth \else
    \vskip \z@ \@plus.2\p@
    {\leftskip #2\relax \rightskip \@tocrmarg \parfillskip -\rightskip
     \parindent #2\relax\@afterindenttrue
     \interlinepenalty\@M
     \leavevmode
     \@tempdima #3\relax
     \advance\leftskip \@tempdima \null\nobreak\hskip -\leftskip
     {#4}\nobreak
     \if@ToCisDotted
     \leaders\hbox{$\m@th
        \mkern \@dotsep mu\hbox{.}\mkern \@dotsep
          mu$}
     \fi
     \hfill
     \nobreak
     \nobreak\hb@xt@\@pnumwidth{\hss#5}
     \par}%
  \fi}


\newcommand*\l@mistix{\@toclinenonbold{1}{5mm}{8mm}}
\newcommand*\l@fhmkyl{\@tocline{1}{5mm}{8mm}}
\renewcommand*\l@section{\@tocline{1}{5mm}{8mm}}
\renewcommand*\l@subsection{\@tocline{2}{10mm}{13mm}}
\renewcommand*\l@subsubsection{\@tocline{3}{15mm}{18mm}}
\renewcommand*\l@paragraph{\@tocline{4}{0mm}{4.1em}}
\renewcommand*\l@table{\@toclinenonbold{1}{0mm}{5.5em}}
\renewcommand*\l@figure\l@table

\renewcommand{\abstract}[1]{\th@StoreIn{\th@abstractStash}{#1}}
\newcommand{\ozet}[1]{\th@StoreIn{\th@ozetStash}{#1}}
\newcommand{\ozgecmis}[1]{\th@StoreIn{\th@ozgecmisStash}{#1}}
\newcommand{\tesekkur}[1]{\@TesekkurVartrue\th@StoreIn{\th@tesekkurStash}{#1}}
\newcommand{\giris}[1]{\th@StoreIn{\th@girisStash}{#1}}
\newcommand{\sembollistesi}[1]{\@SembolListesiVartrue\th@StoreIn{\th@symbollistStash}{#1}}
\newcommand{\kisaltmalar}[1]{\@KisaltmalarVartrue\th@StoreIn{\th@KisaltmalarStash}{#1}}

\def\section{\@ifstar\unnumberedsection\numberedsection}
\def\numberedsection{\@ifnextchar[%]
  \numberedsectionwithtwoarguments\numberedsectionwithoneargument}
\def\unnumberedsection{\@ifnextchar[%]
  \unnumberedsectionwithtwoarguments\unnumberedsectionwithoneargument}
\def\numberedsectionwithoneargument#1{\numberedsectionwithtwoarguments[#1]{#1}}
\def\unnumberedsectionwithoneargument#1{\unnumberedsectionwithtwoarguments[#1]{#1}}
\def\numberedsectionwithtwoarguments[#1]#2{%
  \ifhmode\par\fi
  \removelastskip
  \vspace*{15pt}\goodbreak
  \refstepcounter{section}%
  \noindent
  \leavevmode
  \begingroup
  \th@HdFnt\th@ScSz
  \hspace{-0.75em}
  \begin{tabular}{ll} 
  \thesection.\ & \parbox[t]{\textwidth - \widthof{\thesection.\ } - 1em}{#2}
  \end{tabular}
  \endgroup\goodbreak
  \setlength{\mistixI}{\widthof{#2}}
  \setlength{\mistixII}{\textwidth - \widthof{\thesection.\ }}
  \ifthenelse{\mistixI > \mistixII}{\vspace{1em}}{}
  %\ifthenelse{\widthof{#2} > {\textwidth - \widthof{\thesection.\ }}}{\vspace{1cm}}{\vspace{0cm}}
  \addcontentsline{toc}{section}{\protect\bf\protect\numberline{\thesection.}\protect\bf #1}}
\def\unnumberedsectionwithtwoarguments[#1]#2{%
  \ifhmode\par\fi
  \removelastskip
  \vspace*{15pt}\goodbreak
  \noindent
  \leavevmode
  \begingroup
  \bfseries
  #2\quad
  \endgroup\goodbreak
  \addcontentsline{toc}{section}{\protect\bf #1}}
    
\def\subsection{\@ifstar\unnumberedsubsection\numberedsubsection}
\def\numberedsubsection{\@ifnextchar[%]
  \numberedsubsectionwithtwoarguments\numberedsubsectionwithoneargument}
\def\unnumberedsubsection{\@ifnextchar[%]
  \unnumberedsubsectionwithtwoarguments\unnumberedsubsectionwithoneargument}
\def\numberedsubsectionwithoneargument#1{\numberedsubsectionwithtwoarguments[#1]{#1}}
\def\unnumberedsubsectionwithoneargument#1{\unnumberedsubsectionwithtwoarguments[#1]{#1}}
\def\numberedsubsectionwithtwoarguments[#1]#2{%
  \ifhmode\par\fi
  \removelastskip
   \vspace*{15pt}\goodbreak
  \refstepcounter{subsection}%
  \noindent
  \leavevmode
  \begingroup
  \th@HdFnt\th@ScSz
  \thesubsection.\ 
  #2\quad
  \endgroup\goodbreak
  \addcontentsline{toc}{subsection}{\protect\bf\protect\numberline{\thesubsection.}\protect\bf #1}}
\def\unnumberedsubsectionwithtwoarguments[#1]#2{%
  \ifhmode\par\fi
  \removelastskip
  \vspace*{15pt}\goodbreak
  \noindent
  \leavevmode
  \begingroup
  \bfseries
  #2\quad
  \endgroup\goodbreak
  \addcontentsline{toc}{subsection}{\protect\bf #1}}

\def\subsubsection{\@ifstar\unnumberedsubsubsection\numberedsubsubsection}
\def\numberedsubsubsection{\@ifnextchar[%]
  \numberedsubsubsectionwithtwoarguments\numberedsubsubsectionwithoneargument}
\def\unnumberedsubsubsection{\@ifnextchar[%]
  \unnumberedsubsubsectionwithtwoarguments\unnumberedsubsubsectionwithoneargument}
\def\numberedsubsubsectionwithoneargument#1{\numberedsubsubsectionwithtwoarguments[#1]{#1}}
\def\unnumberedsubsubsectionwithoneargument#1{\unnumberedsubsubsectionwithtwoarguments[#1]{#1}}
\def\numberedsubsubsectionwithtwoarguments[#1]#2{%
  \ifhmode\par\fi
  \removelastskip
   \vspace*{15pt}\goodbreak
  \refstepcounter{subsubsection}%
  \noindent
  \leavevmode
  \begingroup
  \th@HdFnt\th@ScSz
  \thesubsubsection.\ 
  #2\quad
  \endgroup\goodbreak
  \addcontentsline{toc}{subsubsection}{\protect\bf\protect\numberline{\thesubsubsection.}\protect\bf #1}}
\def\unnumberedsubsubsectionwithtwoarguments[#1]#2{%
  \ifhmode\par\fi
  \removelastskip
  \vspace*{15pt}\goodbreak
  \noindent
  \leavevmode
  \begingroup
  \bfseries
  #2\quad
  \endgroup\goodbreak
  \addcontentsline{toc}{subsubsection}{\protect\bf #1}}

\def\paragraph{\@ifstar\unnumberedparagraph\numberedparagraph}
\def\numberedparagraph{\@ifnextchar[%]
  \numberedparagraphwithtwoarguments\numberedparagraphwithoneargument}
\def\unnumberedparagraph{\@ifnextchar[%]
  \unnumberedparagraphwithtwoarguments\unnumberedparagraphwithoneargument}
\def\numberedparagraphwithoneargument#1{\numberedparagraphwithtwoarguments[#1]{#1}}
\def\unnumberedparagraphwithoneargument#1{\unnumberedparagraphwithtwoarguments[#1]{#1}}
\def\numberedparagraphwithtwoarguments[#1]#2{%
  \ifhmode\par\fi
  \removelastskip
   \vspace*{15pt}\goodbreak
  \refstepcounter{paragraph}%
  \noindent
  \leavevmode
  \begingroup
  \th@HdFnt\th@ScSz
  \theparagraph.\ 
  #2\quad
  \endgroup\goodbreak
  \addcontentsline{toc}{paragraph}{\protect\bf\protect\numberline{\theparagraph.}\protect\bf #1}}
\def\unnumberedparagraphwithtwoarguments[#1]#2{%
  \ifhmode\par\fi
  \removelastskip
  \vspace*{15pt}\goodbreak
  \noindent
  \leavevmode
  \begingroup
  \bfseries
  #2\quad
  \endgroup\goodbreak
  \addcontentsline{toc}{paragraph}{\protect\bf #1}}

\let\th@OldAppCmd\appendix
\renewcommand\appendix{
    \@AppendixVartrue
    \addcontentsline{toc}{fhmkyl}{\protect\bf\appendixnameToC}
    \th@OldAppCmd
    \def\thechapter{\preappendixname-\arabic{chapter}}
    \captionsetup [table]{listof=false}
    \captionsetup [figure]{listof=false}
    
}

\renewcommand\chapter{\if@openright\cleardoublepage\else\clearpage\fi
                      \global\@topnum\z@
                      \@afterindentfalse
                      \secdef\@chapter\@schapter}

\def\@chapter[#1]#2{\ifnum \c@secnumdepth >\m@ne
                         \refstepcounter{chapter}%
			\if@AppendixVar\else
                          \addtocontents{toc}{\protect\addvspace{\spacebeforechapterToC}}%
                          \if@ChapterBoldToC
			  \addtocontents{toc}{\protect\parbox{\protect\textwidth}
			  {\protect\centering\protect\bf\thechapter. \protect\bf\chapternameToC \protect\\ \protect\addvspace{\spaceafterchapterToC} \protect\bf #1}
			  }
                          \else
			  \addtocontents{toc}{\protect\parbox{\protect\textwidth}
			  {\protect\centering\protect\thechapter. \protect\chapternameToC \protect\\ \protect\addvspace{\spaceafterchapterToC} \protect #1}
			  }
                          \fi

			\fi
                          %
                    \else
                      \relax
                    \fi
                    \chaptermark{#1}%
                    %LoT ve LoF da bölümler arası geçişde bir boşluk bırakılacaksa...
                    %\addtocontents{lof}{\protect\addvspace{10\p@}}%
                    %\addtocontents{lot}{\protect\addvspace{10\p@}}%
                    \if@twocolumn
                      \@topnewpage[\@makechapterhead{#2}]%
                    \else
                      \@makechapterhead{#2}%
                      \@afterheading
		\fi
                    }

\def\@makechapterhead#1{
    \thispagestyle{empty}%
    \vspace*{\ChapterUstenBosluk}%
    {\parindent \z@ \raggedright
    \normalfont
    \ifnum \c@secnumdepth >\m@ne
	\if@AppendixVar
        \th@HdFnt\th@ScSz \centering \thechapter%\space 
	\else
        \th@HdFnt\th@ScSz \thechapter. \@chapapp %\space 
	\fi
        \par\nobreak
        \vskip 10\p@
    \fi
    \interlinepenalty\@M
    \th@HdFnt\th@ScSz #1\par\nobreak
    \vskip 20\p@
  }}
\def\@schapter#1{
    \@makeschapterhead{#1}
    \@afterheading
    }
    
\def\@makeschapterhead#1{%
  \vspace*{\ChapterUstenBosluk}%
  {\parindent \z@ \raggedright
    \normalfont
    \interlinepenalty\@M
    \th@HdFnt\th@ScSz #1\par\nobreak
    \vskip 20\p@}}
  


\newcommand*{\th@DisKapak}{
    \thispagestyle{empty}
    \clearpage
    \vspace*{-14mm}
    \thispagestyle{empty}
    \begin{list}{}{\setlength{\leftmargin}{0cm}\setlength{\rightmargin}{0cm}}
        \centering\item[]
       % \includegraphics[width=0.2\textwidth]{izu_logo.png} \\
        \th@titleFtSz {T.C. \\ İSTANBUL SABAHATTİN ZAİM ÜNİVERSİTESİ \\ MÜHENDİSLİK VE DOĞA BİLİMLERİ FAKÜLTESİ \\ BİTİRME ÇALIŞMASI \th@raporNo\ RAPORU }
        \vfill
	\th@titleFtSz \th@title \\
        \vfill
        \th@titleFtSz \textbf{Proje Ekibi:} \\
        \th@titleFtSz \th@isimsoyisim \\ 
        \vfill
        \th@titleFtSz Danışman \\
        \th@titleFtSz \th@TeziYoneten \\
        \vfill
        %\th@titleAnaBilimDaliFtSz \th@AnaBilimDali \ Anabilim Dalı\\
        \th@titleAnaBilimDaliFtSz Elektrik-Elektronik Mühendisliği \\ Bitirme Ödevi \\
        \vfill
        \th@titleFtSz \th@date \\
        \th@titleFtSz ISTANBUL \\
    \end{list}}


\newcommand*{\th@IcKapak}{
    %\clearpage
    \vspace*{-14mm}
    \thispagestyle{empty}
    \begin{list}{}{\setlength{\leftmargin}{0cm}\setlength{\rightmargin}{0cm}}
    	\centering\item[]
    	\th@titleFtSz {T.C. \vspace*{1mm}\\ İSTANBUL SABAHATTİN ZAİM ÜNİVERSİTESİ \vspace*{1mm}\\ MÜHENDİSLİK VE DOĞA BİLİMLERİ FAKÜLTESİ }
    	\vspace{0.1cm}
        \includegraphics[width=0.2\textwidth]{izu_logo.png} \\
        \vspace{1cm}
    	\th@titleFtSz \th@title \\
    	\vfill
    	\th@titleFtSz \textbf{Proje Ekibi:} \\ 
    	\vspace*{5mm}
    	\th@titleFtSz \th@isimsoyisim \\
    	\vspace*{1mm}	
    	\th@titleFtSz \th@numaraBir \\
    	\vspace*{1mm}
    	
		\if@AuthorIkiVar
		\th@titleFtSz \th@isimsoyisimIki \vspace*{1mm} \\ \th@titleFtSz \th@numaraIki \vspace*{1mm} \vspace*{1mm}\\  \fi
		
		
		\if@AuthorUcVar
		\th@titleFtSz \th@isimsoyisimUc \vspace*{1mm}\\ \th@titleFtSz \th@numaraUc \vspace*{1mm}\\ \fi
		
		
		\if@AuthorDortVar
		\th@titleFtSz \th@isimsoyisimDort \vspace*{1mm}\\ \th@titleFtSz \th@numaraDort \vspace*{1mm}\\ \fi

        \if@AuthorBesVar
        \th@titleFtSz \th@isimsoyisimBes \vspace*{1mm}\\ \th@titleFtSz \th@numaraBes \vspace*{1mm}\\ \fi
    	
    	
    	\vfill
    	\th@titleFtSz Proje Danışmanı \vspace*{1mm}\\
    	\th@titleFtSz \th@TeziYoneten \\
    	\vfill
    	
    	\th@titleAnaBilimDaliFtSz Lisans, Elektrik-Elektronik Mühendisliği \vspace*{1mm} %\\ EEM 401 Bitirme Çalışması I - \th@raporNo\ Raporu \\
    	\vfill
    	
    	
    
   	\vfill
    	\th@titleFtSz \th@date \vspace*{1mm}\\
    	\th@titleFtSz İSTANBUL \\
\end{list}}


 \newcommand{\th@tezOneriPage}{
    \thispagestyle{empty}
	\hfill 06/03/2019
 	\vspace*{3mm}
 	\begin{center}
 		\textbf{T.C.}\\
 		\textbf{İSTANBUL SABAHATTİN ZAİM ÜNİVERSİTESİ ÜNİVERSİTESİ}\\
 		\textbf{MÜHENDİSLİK VE DOĞA BİLİMLERİ FAKÜLTESİ}\\
        \includegraphics[width=0.2\textwidth]{izu_logo.png}
 		\textbf{ELEKTRİK-ELEKTRONİK MÜHENDİSLİĞİ BÖLÜMÜ}\\
 		\vspace{1cm}
 		\textbf{\th@AnaBilimDali\ Bitirme Çalışması \th@raporNo\ Rapor}\\ %%%%%%%%%%%%%%%%%%%%%%%*******%%%%%%%%%%%%
 	\end{center}
 	 \textbf{Eğitim Öğretim Yılı:}  \th@Yil\ Yılı  \th@Donem\ Dönemi\\
 	 \textbf{Proje Adı:}  \th@titleCase \\
 	
 	\begin{tabular}{llllp{3cm}}
  		\underline{\textbf{Proje Ekibi}} &&&&\\
 	\textbf{Ad ve Soyad}&\textbf{Numara}&\textbf{E-mail}&\textbf{Bölüm}&\hfill \\\\
 				 \th@isimsoyisim\ & \th@numaraBir &\th@mailBir &\th@bolumBir& \\\\
\if@AuthorIkiVar
\th@isimsoyisimIki\ & \th@numaraIki &\th@mailIki &\th@bolumIki& \\ \\  \fi 				 
 \if@AuthorUcVar
\th@isimsoyisimUc\ & \th@numaraUc &\th@mailUc &\th@bolumUc&\\ \\ \fi 	
 \if@AuthorDortVar
 \th@isimsoyisimDort\ & \th@numaraDort &\th@mailDort &\th@bolumDort&\\  \fi
  \if@AuthorBesVar
 \th@isimsoyisimBes\ & \th@numaraBes &\th@mailBes &\th@bolumBes&\\  \fi 				 
 		
 	\end{tabular}
 \\
 \vfill 
 \textbf{Danışman}\\
 \th@TeziYoneten	
 	 \newpage
 	
 } 

\newcommand{\th@Abstractpage}{
  \newpage
    \clearpage
%    \thispagestyle{empty}
    \addcontentsline{toc}{mistix}{\protect\abstractnameToC}
    \begin{list}{}{\setlength{\leftmargin}{0cm}\setlength{\rightmargin}{0cm}}
    \centering\item[]
      \vfill
        \th@ChapterStyle \th@titleenglish\\
        \vspace*{5mm}
        \textbf \makebox{\th@isimsoyisim}\\
        \vspace*{2pt}
        
        \textbf \makebox{Istanbul Sabahattin Zaim University, Computer Engineering} \\
        \vspace*{2pt}
        \textbf \makebox{Supervisor: \th@TeziYonetenenglish} \\
        \vspace*{5mm}
        \th@ChapterStyle ABSTRACT\\
    \end{list}
    \vspace*{-5mm}
    \sloppy
    \th@abstractStash   % the actual abstract
}

\newcommand{\th@ozetpage}{
  \newpage
    \clearpage
%    \thispagestyle{empty}
    \addcontentsline{toc}{mistix}{\protect\ozetnameToC}
    \begin{list}{}{\setlength{\leftmargin}{0cm}\setlength{\rightmargin}{0cm}}
    \centering\item[]
        \vfill
        \th@ChapterStyle \th@title\\
        \vspace*{5mm}
        \textbf \makebox{\th@isimsoyisim}\\
        \vspace*{2pt}
        \textbf \makebox{İstanbul Sabahattin Zaim Üniversitesi, Elektrik-Elektronik Mühendisliği} \\
        \vspace*{2pt}
        \textbf \makebox{Danışman : \th@TeziYoneten} \\
        \vspace*{5mm}
        \th@ChapterStyle ÖZET\\
    \end{list}
    \vspace*{-7mm}
    \sloppy
    \th@ozetStash   % the actual ozet
}

\newcommand{\th@ozgecmispage}{
    \clearpage
%    \thispagestyle{empty}
    \addcontentsline{toc}{fhmkyl}{\protect\bf\ozgecmisnameToC}%
    \begin{list}{}{\setlength{\leftmargin}{0cm}\setlength{\rightmargin}{0cm}}
    \centering\item[]
%       \vspace*{70mm}
        \th@ChapterStyle ÖZGEÇMİŞ\\
    \end{list}
    \vspace*{-5mm}
    \sloppy
    \th@ozgecmisStash   % the actual ozgecmis
}


\newcommand{\th@tesekkurpage}{
    \clearpage
%    \thispagestyle{empty}
    \addcontentsline{toc}{mistix}{\protect\tesekkurnameToC}%
    \begin{list}{}{\setlength{\leftmargin}{0cm}\setlength{\rightmargin}{0cm}}
    \centering\item[]
%       \vspace*{70mm}
        \th@ChapterStyle ÖNSÖZ / TEŞEKKÜR\\
    \end{list}
    \vspace*{-5mm}
    \sloppy
    \th@tesekkurStash   % the actual tesekkur
}


\renewcommand\tableofcontents{
    \setlength{\ChapterUstenBosluk}{-5mm}
    
    \chapter*{\centering\fontfamily{cmr}\normalsize\selectfont İÇİNDEKİLER\normalfont\@mkboth{\MakeUppercase\contentsname}{\MakeUppercase\contentsname}}
    \pagestyle{empty}
    \addcontentsline{toc}{mistix}{\protect\contentsnameToC}
   \begin{center}
    \vspace*{-5mm}
    \th@HdFnt \th@title \\
    \vspace*{-5mm}
    %\vspace*{5mm}
    \end{center}
    \fontfamily{cmr}\normalfont\normalsize\
    \setlength{\ChapterUstenBosluk}{30mm}
    \newlength{\th@oldparskip}
    \setlength{\th@oldparskip}{\parskip}
    \setlength{\parskip}{\ToCparskip}
    \@starttoc{toc}
    \setlength{\parskip}{\th@oldparskip}
    
    }
  
\newcommand\symbollist{
    \setlength{\ChapterUstenBosluk}{-5mm}
    \chapter*{\symbolname\@mkboth{\MakeUppercase\symbolname}{\MakeUppercase\symbolname}}
    \setlength{\ChapterUstenBosluk}{30mm}
    \addcontentsline{toc}{mistix}{\protect\symbolnameToC}
    \th@symbollistStash}

\newcommand\abbreviation{
    \setlength{\ChapterUstenBosluk}{-5mm}
    \chapter*{\abbreviationname\@mkboth{\MakeUppercase\abbreviationname}{\MakeUppercase\abbreviationname}}
    \setlength{\ChapterUstenBosluk}{30mm}
    \addcontentsline{toc}{mistix}{\protect\abbreviationnameToC}
    \th@KisaltmalarStash}

\renewcommand\listoftables{
    \setlength{\ChapterUstenBosluk}{-5mm}
    \chapter*{\listtablename}
    \setlength{\ChapterUstenBosluk}{30mm}
    \@mkboth{\MakeUppercase\listtablename}{\MakeUppercase\listtablename}
    \let\orig@numberline\numberline
    \renewcommand*{\numberline}[1]{%
    \orig@numberline{\tablename\ ##1.}}%
    \addcontentsline{toc}{mistix}{\protect\listtablenameToC}
    \setlength{\th@oldparskip}{\parskip}
    \setlength{\parskip}{\LoTparskip}
    \@starttoc{lot}
    \setlength{\parskip}{\th@oldparskip}
    \let\numberline\orig@numberline
    }
    
\renewcommand\listoffigures{
    \setlength{\ChapterUstenBosluk}{-5mm}
    \chapter*{\listfigurename}
    \setlength{\ChapterUstenBosluk}{30mm}
    \@mkboth{\MakeUppercase\listfigurename}{\MakeUppercase\listfigurename}
    \let\orig@numberline\numberline
    \renewcommand*{\numberline}[1]{%
    \orig@numberline{\figurename\ ##1.}}%
    \addcontentsline{toc}{mistix}{\protect\listfigurenameToC}
    \setlength{\th@oldparskip}{\parskip}
    \setlength{\parskip}{\LoFparskip}
    \@starttoc{lof}
    \setlength{\parskip}{\th@oldparskip}
    \let\numberline\orig@numberline
    }
    

\renewcommand{\@biblabel}[1]{[#1]}

\renewenvironment{thebibliography}[1]
     {
     \setlength{\ChapterUstenBosluk}{-5mm}
    \chapter*{\fontfamily{cmr}\normalsize\selectfont KAYNAKLAR\normalfont\@mkboth{\MakeUppercase\     
    contentsname}{\MakeUppercase\contentsname}}
% chapter*{\bibname}%
     \setlength{\ChapterUstenBosluk}{30mm}
     \addcontentsline{toc}{fhmkyl}{\protect\bf\bibnameToC}
      \@mkboth{\MakeUppercase\bibname}{\MakeUppercase\bibname}%
      \list{\@biblabel{\@arabic\c@enumiv}}%
           {\settowidth\labelwidth{\@biblabel{#1}}%
            \leftmargin\labelwidth
            \advance\leftmargin\labelsep
	    \advance\leftmargin by 1.5em
	    \setlength\itemindent{-1.5em}
            \@openbib@code
            \usecounter{enumiv}%
            \let\p@enumiv\@empty
            \renewcommand\theenumiv{\@arabic\c@enumiv}}%
      \sloppy
      \clubpenalty4000
      \@clubpenalty \clubpenalty
      \widowpenalty4000%
      \sfcode`\.\@m
	 }
     {\def\@noitemerr
       {\@latex@warning{Empty `thebibliography' environment}}%
      \endlist}

\newcommand{\th@MakeThesisPrologue}{
        \fontfamily{cmr}
        \selectfont
        %\th@DisKapak
       % \th@tezOneriPage
        \th@IcKapak
        \normalfont
        \pagenumbering{roman}%
        \setcounter{page}{0}
        \ToCspacing
        \tableofcontents
        \listoftables
        \listoffigures
        \symbollist
        \abbreviation
	\clearpage
	\pagestyle{fancy}
       
        \newpage
	\pagenumbering{arabic}
        \oneandonehalf
%	\th@girispage
        \oneandonehalf
        }

\let\th@OldBeginDocument\document
\renewcommand*{\document}{
    \th@OldBeginDocument
    \th@MakeThesisPrologue
}
\endinput
