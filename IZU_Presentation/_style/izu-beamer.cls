% izu-beamer.cls — İstanbul Sabahattin Zaim Üniversitesi Beamer Sınıfı
% Sıfırdan yazılmıştır. Herhangi bir üçüncü taraf şablonundan türetilmemiştir.

\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{_style/izu-beamer}[2026/06/08 v1.0 IZU Beamer Sunum Sinifi]

\DeclareOption*{\PassOptionsToClass{\CurrentOption}{beamer}}
\ProcessOptions\relax
\LoadClass{beamer}

% ---------------------------------------------------------------
% Temel paketler
% ---------------------------------------------------------------
\usepackage[utf8]{inputenc}
\usepackage[T1]{fontenc}
\usepackage[shorthands=off,turkish]{babel} % shorthands=off: '=' aktif olmasin, key=value bozulmasin
\usepackage{lmodern}
\usepackage{microtype}
\usepackage{amsmath,amssymb}
\usepackage{graphicx}
\usepackage{tikz}
\usetikzlibrary{positioning,shapes,shadows,arrows.meta,calc}
\usepackage{booktabs}
\usepackage{xcolor}
\usepackage{hyperref}

% ---------------------------------------------------------------
% İZÜ Renk Paleti  (logo'dan örneklenen bordo + altın)
% ---------------------------------------------------------------
\definecolor{izuBordo}{HTML}{6E1A2E}   % ana bordo
\definecolor{izuDark}{HTML}{3D0A17}    % koyu bordo (panel arka planı)
\definecolor{izuGold}{HTML}{C8A040}    % altın vurgu
\definecolor{izuCream}{HTML}{FDF6F0}   % açık krem (içerik arka planı)
\definecolor{izuLightRed}{HTML}{F2E0E4}% blok arka planı
\definecolor{izuGrey}{HTML}{3A3A3A}    % metin rengi

% ---------------------------------------------------------------
% Genel Beamer renk teması
% ---------------------------------------------------------------
\setbeamercolor{background canvas}{bg=izuCream}
\setbeamercolor{normal text}{fg=izuGrey}
\setbeamercolor{structure}{fg=izuBordo}

\setbeamercolor{palette primary}{bg=izuBordo,   fg=white}
\setbeamercolor{palette secondary}{bg=izuDark,  fg=white}
\setbeamercolor{palette tertiary}{bg=izuGold,   fg=white}

\setbeamercolor{frametitle}{bg=izuBordo, fg=white}
\setbeamercolor{title}{fg=white}
\setbeamercolor{subtitle}{fg=izuGold}
\setbeamercolor{author}{fg=white}
\setbeamercolor{institute}{fg=white!85!izuBordo}
\setbeamercolor{date}{fg=izuGold}

\setbeamercolor{block title}{bg=izuBordo, fg=white}
\setbeamercolor{block body}{bg=izuLightRed, fg=izuGrey}
\setbeamercolor{block title alerted}{bg=izuDark, fg=white}
\setbeamercolor{block body alerted}{bg=izuLightRed, fg=izuGrey}
\setbeamercolor{block title example}{bg=izuGold, fg=white}
\setbeamercolor{block body example}{bg=izuCream, fg=izuGrey}

\setbeamercolor{itemize item}{fg=izuBordo}
\setbeamercolor{itemize subitem}{fg=izuGold}
\setbeamercolor{itemize subsubitem}{fg=izuGrey}
\setbeamercolor{enumerate item}{fg=izuBordo}

\setbeamercolor{footline}{bg=izuDark, fg=white}
\setbeamercolor{section in head/foot}{bg=izuBordo, fg=white}

% ---------------------------------------------------------------
% Font teması
% ---------------------------------------------------------------
\setbeamerfont{title}{size=\Large,  series=\bfseries}
\setbeamerfont{subtitle}{size=\small, series=\mdseries}
\setbeamerfont{author}{size=\normalsize, series=\bfseries}
\setbeamerfont{institute}{size=\scriptsize}
\setbeamerfont{date}{size=\scriptsize}
\setbeamerfont{frametitle}{size=\large, series=\bfseries}
\setbeamerfont{framesubtitle}{size=\small, series=\mdseries}
\setbeamerfont{footline}{size=\tiny}
\setbeamerfont{block title}{size=\normalsize, series=\bfseries}

% ---------------------------------------------------------------
% Madde işaretleri
% ---------------------------------------------------------------
\setbeamertemplate{itemize item}{%
  \tikz[baseline=-0.55ex]{\fill[izuBordo] (0,0) rectangle (0.55ex,0.55ex);}}
\setbeamertemplate{itemize subitem}{%
  \tikz[baseline=-0.55ex]{\fill[izuGold] (0,0) circle (0.32ex);}}
\setbeamertemplate{itemize subsubitem}{%
  \tikz[baseline=-0.55ex]{\draw[izuGrey,thick] (0,0) -- (0.6ex,0);}}
\setbeamertemplate{navigation symbols}{}

% ---------------------------------------------------------------
% Başlık sayfası
% ---------------------------------------------------------------
\setbeamertemplate{title page}{%
  \begin{tikzpicture}[remember picture, overlay]
    %% Sol koyu panel (% 52 genişlik)
    \fill[izuDark]
      (current page.north west) rectangle
      ([xshift=0.52\paperwidth] current page.south west);
    %% Altın dikey şerit
    \fill[izuGold]
      ([xshift=0.52\paperwidth] current page.north west) rectangle
      ([xshift=0.526\paperwidth] current page.south west);
    %% Sol üst köşe: İZÜ logosu
    \node[anchor=north west, inner sep=0pt] at
      ([xshift=0.025\paperwidth, yshift=-0.025\paperheight] current page.north west)
      {\includegraphics[height=0.10\paperheight]{izu_logo}};
    %% Sağ panel arka planı (krem)
    \fill[izuCream]
      ([xshift=0.526\paperwidth] current page.north west) rectangle
      (current page.south east);
    %% Sağ panel başlık şeridi
    \fill[izuBordo]
      ([xshift=0.526\paperwidth, yshift=0pt] current page.north west) rectangle
      ([xshift=\paperwidth,      yshift=-0.13\paperheight] current page.north west);
    \node[anchor=center, text=white, font=\small\bfseries] at
      ([xshift=0.763\paperwidth, yshift=-0.065\paperheight] current page.north west)
      {Kullanılan Teknolojiler};
    %% 2 sütun x 4 satır logo grid
    %% Sütun x konumları: sol=0.635, sağ=0.885
    %% Satır y konumları: 0.28, 0.49, 0.70, 0.88
    % --- Satır 1 ---
    \node[anchor=center] at
      ([xshift=0.635\paperwidth, yshift=-0.28\paperheight] current page.north west)
      {\includegraphics[height=0.10\paperheight]{st_logo}};
    \node[anchor=center] at
      ([xshift=0.885\paperwidth, yshift=-0.28\paperheight] current page.north west)
      {\includegraphics[height=0.10\paperheight]{cubemx}};
    \draw[izuGold!40, thin]
      ([xshift=0.555\paperwidth, yshift=-0.385\paperheight] current page.north west) --
      ([xshift=0.975\paperwidth, yshift=-0.385\paperheight] current page.north west);
    % --- Satır 2 ---
    \node[anchor=center] at
      ([xshift=0.635\paperwidth, yshift=-0.49\paperheight] current page.north west)
      {\includegraphics[height=0.10\paperheight]{FreeRTOS_Logo}};
    \node[anchor=center] at
      ([xshift=0.885\paperwidth, yshift=-0.49\paperheight] current page.north west)
      {\includegraphics[height=0.10\paperheight]{C_Programming_Language_logo}};
    \draw[izuGold!40, thin]
      ([xshift=0.555\paperwidth, yshift=-0.595\paperheight] current page.north west) --
      ([xshift=0.975\paperwidth, yshift=-0.595\paperheight] current page.north west);
    % --- Satır 3 ---
    \node[anchor=center] at
      ([xshift=0.635\paperwidth, yshift=-0.70\paperheight] current page.north west)
      {\includegraphics[height=0.10\paperheight]{rocketpy_logo}};
    \node[anchor=center] at
      ([xshift=0.885\paperwidth, yshift=-0.70\paperheight] current page.north west)
      {\includegraphics[height=0.10\paperheight]{Python-logo}};
    \draw[izuGold!40, thin]
      ([xshift=0.555\paperwidth, yshift=-0.805\paperheight] current page.north west) --
      ([xshift=0.975\paperwidth, yshift=-0.805\paperheight] current page.north west);
    % --- Satır 4 ---
    \node[anchor=center] at
      ([xshift=0.763\paperwidth, yshift=-0.895\paperheight] current page.north west)
      {\includegraphics[height=0.08\paperheight]{Visual_Studio_Code_logo}};
    %% Sol panel metin içeriği — TikZ node, tam konum kontrolü
    \node[anchor=north west, text width=0.46\paperwidth,
          inner sep=0pt, align=center] at
      ([xshift=0.03\paperwidth, yshift=-0.22\paperheight] current page.north west)
      {%
        \hyphenpenalty=10000\exhyphenpenalty=10000\sloppy
        {\usebeamerfont{title}\color{white}\centering\inserttitle}\par
        \vspace{0.18cm}
        \textcolor{izuGold}{\rule{7em}{0.8pt}}\par
        \vspace{0.15cm}
        {\usebeamerfont{subtitle}\color{white!80!izuDark}\centering\insertsubtitle}\par
        \vspace{0.35cm}
        {\usebeamerfont{author}\color{white}\insertauthor}\par
        \vspace{0.12cm}
        {\usebeamerfont{institute}\color{white!70!izuDark}\insertinstitute}\par
        \vspace{0.12cm}
        {\usebeamerfont{date}\color{izuGold}\insertdate}%
      };
  \end{tikzpicture}%
}

% ---------------------------------------------------------------
% Frame başlığı
% ---------------------------------------------------------------
\setbeamertemplate{frametitle}{%
  \nointerlineskip
  \begin{beamercolorbox}[wd=\paperwidth, ht=0.60cm, dp=0.18cm, leftskip=0.45cm]{frametitle}
    \usebeamerfont{frametitle}\insertframetitle%
    \ifx\insertframesubtitle\@empty\else
      \enspace{\usebeamerfont{framesubtitle}\color{izuGold}— \insertframesubtitle}%
    \fi
  \end{beamercolorbox}
  \nointerlineskip
  % Altın alt çizgi
  \begin{beamercolorbox}[wd=\paperwidth, ht=1.8pt, dp=0pt]{palette tertiary}
  \end{beamercolorbox}
}

% ---------------------------------------------------------------
% Footer
% ---------------------------------------------------------------
\setbeamertemplate{footline}{%
  \begin{beamercolorbox}[wd=\paperwidth, ht=0.42cm, dp=0.15cm]{footline}
    \hspace{0.4cm}%
    % Sol: logo yazısı + yazar
    {\color{izuGold}\textbf{İZÜ}}\enspace
    {\color{white!65!izuDark}\insertshortauthor}%
    \hfill
    % Orta: kısa başlık
    {\color{white!80!izuDark}\insertshorttitle}%
    \hfill
    % Sağ: tarih + sayfa
    {\color{white!65!izuDark}\insertshortdate}\enspace
    {\color{white}\bfseries\insertframenumber\,/\,\inserttotalframenumber}%
    \hspace{0.4cm}
  \end{beamercolorbox}
}

% ---------------------------------------------------------------
% Bölüm başlangıç slaytı  (başlık sayfasıyla tutarlı split-panel)
% ---------------------------------------------------------------
\AtBeginSection[]{%
  \begin{frame}[plain]
    \begin{tikzpicture}[remember picture, overlay]
      %% Sol bordo panel (%40 genişlik)
      \fill[izuBordo]
        (current page.north west) rectangle
        ([xshift=0.40\paperwidth] current page.south west);
      %% Altın dikey bölücü
      \fill[izuGold]
        ([xshift=0.400\paperwidth] current page.north west) rectangle
        ([xshift=0.407\paperwidth] current page.south west);
      %% Sağ koyu panel
      \fill[izuDark]
        ([xshift=0.407\paperwidth] current page.north west) rectangle
        (current page.south east);
      %% Sol: "BÖLÜM" etiketi
      \node[text=izuGold, font=\normalsize\bfseries, anchor=south] at
        ([xshift=0.200\paperwidth, yshift=-0.385\paperheight] current page.north west)
        {BÖLÜM};
      %% Sol: büyük bölüm numarası
      \node[text=white, font=\fontsize{72}{72}\selectfont\bfseries, anchor=north] at
        ([xshift=0.200\paperwidth, yshift=-0.410\paperheight] current page.north west)
        {\thesection};
      %% Sağ: bölüm başlığı
      \node[text=white, font=\LARGE\bfseries, anchor=center,
            text width=0.53\paperwidth, align=center] at
        ([xshift=0.70\paperwidth, yshift=-0.50\paperheight] current page.north west)
        {\insertsectionhead};
      %% Sağ: altın dekoratif çizgi
      \draw[izuGold, line width=1pt]
        ([xshift=0.455\paperwidth, yshift=-0.645\paperheight] current page.north west) --
        ([xshift=0.960\paperwidth, yshift=-0.645\paperheight] current page.north west);
    \end{tikzpicture}
  \end{frame}
}
