В этом файле описывается процесс установки и настройки
фидонет-совместимого мейлера bforce 0.22.8.ugenk1.

В данном документе приняты следующие обозначения:

<SRCDIR> - путь, куда вы распаковали тарболл с исходными
	   текстами bforce 0.22.8.ugenk1 (далее bforce)
Тарболл  - файл с расширением tar.gz, или tar.bz2


Требования к системе
====================

Для компиляции bforce вам потребуется компилятор (для 
gnu/bsd-систем gcc), так же GNU make (make для линукс,
gmake для bsd).

Обратитесь к документации по вашей системе того, что бы
узнать как установить вышеперечисленное программное обеспечение.


Процесс компиляции
==================

Распакуйте тарболл следующими командами:

gzip -d bforce-0.22.8.ugenk1.tar.gz
или 
bzip2 -d bforce-0.22.8.ugenk1.tar.bz2

tar -xvf bforce-0.22.8.ugenk1.tar

Перейдите в директорию bforce-0.22.8.ugenk1/source:
cd <SRCDIR>/bforce-0.22.8.ugenk1/source

Для создания Makefile, который наиболее подходит к вашей
системе, наберите:

./configure --help

и внимательно прочитайте предлагаемую справку.
Если вам не понятна предлагаемая справка, то просто запустите:

./configure

## WARNING! Внимание!
## Новые версии bforce собираются с поддержкой syslog по умолчанию.
## Если вы не знаете что это/зачем вам это нужно - тогда делайте
## ./configure --disable-syslog

При необходимости исправьте Makefile для ваших нужд.
Запустите команду make (для bsd - gmake):

make

В случае ошибок при выполнении команды make пишите bugreport на
e.kozhuhovskiy@gmail.com или в эхоконференцию ru.unix.ftn

Возможно, прийдется пересоздать configure с помощью autoconf

Поздравляем, компиляция завершилась успешно.
Для установки bforce наберите:

make install

Для bsd:

gmake install


Настройка
=========

Для нормальной работы вам нужно отредактировать конфигурационные файлы
bforce. По умолчанию они находятся в /usr/local/fido/etc и называются
bforce.conf - основной конфигурационный файл bforce
bforce.passwd - файл определения паролей на сессии
bforce.subst - файл переопределения данных из nodelist
freq.dirs - файл задания списка директорий для freq-запросов
freq.aliases - файл задания magic freqests

Так же вам необходимо подправить файл outman. По умолчанию он лежит в
/usr/local/fido/bin. outman - это скрипт, вызываемый вами для создания
прозвонок, файловых запросов и файл-аттачей на узлы ftn.


Использование
=============

bforce - основной файл bforce, служит для запуска
демона, исходящих звонков и запуска из-под mgetty\portslave для приема
входящих звонков.
Попробуйте bforce --help


bfindex - служит для обновления индексных файлов (перекомпиляции)
нодлистов. Испрользование: bfindex

bfstat - служит для показа почтовой очереди. Использование: bfstat

nlookup - служит для поиска информации в нодлисте. Использование:
nlookup <ftn_address>

outman - см. "Настройка". Попробуйте outman --help

В папке debian лежат man-страницы.
TODO: дописать Makefile для установки man-pages

Совместная работа с mgetty
==========================
Для того, что бы узел мог отвечать на входящие звонки, необходимо установить
соотв. програмное обеспечение. Наиболее распространенная программа - mgetty.
Для того, что бы она могла работать на ftn-узле, необходимо собрать ее с опцией
-DFIDO. В случае дистрибутивной сборки это можно проверить с помощью
cat `which mgetty` |strings |grep FIDO |wc -l
Если вы получите число, отличное от нуля, то вам повезло :)

В login.config допишите следующую конструкцию:
/FIDO/  news       fido    /usr/bin/bforce @

где news - это пользователь, из-под которого у вас работает ftn-подсистема.

Совместная работа с inetd/xinetd
================================
К сожалению (или к счастью), bforce не умеет самостоятельно принимать входящие
binkd/ifc вызовы. Для этого вам прийдется использовать суперсервер интернета -
inetd или xinetd.
Вот примеры для inetd:
binkp stream tcp nowait news /usr/bin/bforce bforce -i binkp
ifc   stream tcp nowait news /usr/bin/bforce bforce -i auto

Проверьте, что бы у вас были соотв. строки в /etc/services, например:
binkp           24554/tcp                       # binkp fidonet protocol
ifc             60179/tcp                       # fidonet EMSI over TCP

Дополнительные утилиты
======================

Дополнительные утилиты для bforce находятся в <SRCDIR>/contrib:
bflan			   - bforce log analyzer
callout.sh		   - скрипт для отзвонки на аплинков
outman		 	   - скрипт outman
timesync.tcl		   - скрипт для синхорнизации времени с узлами ftn.
init.d/bforce		   - init-скрипт для RedHat
bfha			   - bforce history analyzer (bfha)
bfha/README		   - bfha README
bfha/bfha.pl		   - собственно, bfha
u-srif		 	   - продвинутый freq-процессор
u-srif/u-srif-index.py     \ с поддержой отчетов,
u-srif/u-srif-lookup.py     \ ограничений, 
u-srif/u-srif.py	     \ индексации файловой базы,
u-srif/conf		      \ что значительно ускоряет
u-srif/conf/report.footer      \ работу.
u-srif/conf/report.header       \ Написан на python.
u-srif/conf/u-srif.aliases	 \ --------------------
u-srif/conf/u-srif.conf	   	  \ -------------------
u-srif/conf/u-srif.dirs	 	   \ ------------------
u-srif/lib			   / ------------------
u-srif/lib/uconfig.py		  / -------------------
u-srif/lib/udbase.py		 / --------------------
u-srif/lib/ufido.py		/ ---------------------
u-srif/lib/unodestat.py        / ----------------------
u-srif/lib/utmpl.py	      / -----------------------
u-srif/lib/uutil.py	     / ------------------------
