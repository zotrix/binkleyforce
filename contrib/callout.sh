#!/bin/sh

Ver=1.1h

# Скрипт для прозвонки на аплинков
# Про время их работы (и телефоны хидденов) должен знать мейлер
#
# Заточен для мейлера BinkleyForce
# Для других править функцию docallout

#
# Copyright (c) 2000 by Georgi Fofanov, 2:5050/29@fidonet
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#

# Путь к outbound'у нужной зоны (пока знаем только про одну зону)
outb=/var/spool/ifmail/outb/

# Путь к каталогу с lock-файлами (последний слэш нужен!)
lock=/var/lock/

# Пауза между звонками
# В секундах
dialdelay=120

# Период сканирования outbound'а
# Пока после завершения круга прозвонки будет 2 паузы - dialdelay и scan
# В секундах
scan=60

# Пауза между проверками lock-файла
# В секундах
lockdelay=10

# Расположение мейлера
mailer=/usr/local/bin/bforce

# Команда, выполняемая после успешной сессии
after=/usr/local/bin/after_session

# Устройство, на котором сидит модем
TTY="ttyS1"

# Здесь перечислены системы, на которые можно звонить и (через пробел) число
# их модемных линий
# Например:
# 2:5050/13 3
# 2:5050/9 2
# 2:5050/33 1
sys_poll=/etc/fido/poll.list

# Пользователь, от имени которого надо запускать
mailer_owner=fido

# Временный файл, куда записывается список ?lo и ?ut
list_file=/var/spool/ifmail/list.lst

# Временный файл
tmp_file=/var/spool/ifmail/list.tmp

# Временный файл, в котором перечислены системы, на которые будем звонить
poll_file=/var/spool/ifmail/polling.list

# Максимальное число попыток дозвониться на систему
MAXTRY=25

# Сколько времени не звонить на систему, если исчерпано число попыток дозвона
# В минутах
MAXTIME=60

# Производим звонок
function docallout ()
{
    local zone=$1 net=$2 node=$3 point=$4 curtry=$5 numline=$6 line
  
  let line=${curtry}%${numline}

  if [ $point = 0 ] ; then
    echo -n `date +%b\ %m\ %T` callout[$$] Звоним на ${zone}:${net}/${node} \(try \#${curtry}\)
    $mailer ${zone}:${net}/${node} -l $line
    let result=$?
  else
    echo -n `date +%b\ %m.%Y\ %T` callout[$$] Звоним на ${zone}:${net}/${node}.${point} \(try \#${curtry}\)
    $mailer ${zone}:${net}/${node}.${point} -l $line
    let result=$?
  fi

  case $result in
    0 ) echo " успешно" ;;
    * ) echo " ошибка" \#$result ;;
  esac

  if [ $result = 0 ] ; then
    echo `date +%s` 0 0 >$sts
    `$after`
  else
    echo `date +%s` $curtry $result >$sts
  fi
}

# Проверяем, можно ли звонить на эту систему
function checkcallout ()
{
    local curtry=$1 zone=$2 net=$3 node=$4 point=$5 numline=0

  if [ $point = 0 ] ; then
    `fgrep "${zone}:${net}/${node}" $sys_poll | awk '{ print "let numline=" $2 }'`
  else
    `fgrep "${zone}:${net}/${node}.${point}" $sys_poll | awk '{ print "let numline=" $2 }'`
  fi
  if [ $numline == 0 ] ; then return ; fi

  docallout $zone $net $node $point $curtry $numline
}

# Сканируем outbound
function scandir ()
{
  find -type f -and \( -name "*.?lo" -o -name "*.?ut" \) > $list_file
  for file in `cat $list_file` ; do
    eval `echo $file | awk '{ sub(/\.\//, "")
      if (substr($0, 9, 4) == ".pnt") {
        point = substr($0, 18, 4)
      } else {
        point = 0
      }
      printf "zonehex=%s nethex=%s nodehex=%s pointhex=%s let zone=0x%s net=0x%s node=0x%s point=0x%s", 
      "2", substr($0, 1, 4), substr($0, 5, 4), point,
      "2", substr($0, 1, 4), substr($0, 5, 4), point 
      }'`
    ext=${file:${#file}-3}
    sts=${file%%?ut}
    sts=${sts%%?lo}
    bsy=${sts}bsy
    sts=${sts}sts
    if [ $ext != hlo -a $ext != hut ] ; then
      echo ${zone} ${net} ${node} ${point} $sts $bsy >> $tmp_file
    fi
  done

  rm $list_file

  if [ -f $tmp_file ] ; then
    `cat $tmp_file | sort | uniq > $poll_file`
    rm $tmp_file
    n=0
    for sys in `cat $poll_file` ; do
      let n=${n}+1
      let tmp=${n}%6
      case $tmp in
        1 ) let zone=$sys ;;
        2 ) let net=$sys ;;
        3 ) let node=$sys ;;
        4 ) let point=$sys ;;
        5 ) sts=$sys ;;
        0 ) bsy=$sys ;;
      esac
      if [ $tmp == 0 ] ; then
        while [ -e ${lock}LCK..$TTY ]
        do
          sleep $lockdelay
        done
        if [ ! -f $bsy ] ; then
          if [ ! -f $sts ] ; then 
            let lasttime=0 retries=0 errcode=0
          else
            `cat $sts|awk '{ print "let lasttime=" $1 " retries=" $2 " errcode=" $3 }'`
          fi
          let curtime=`date +%s`
          let timediff=${curtime}-${lasttime}
          let curtry=${retries}+1
          if [ $curtry -gt $MAXTRY ] ; then
	    if [ $timediff -gt $MAXTIME ] ; then
	      let curtry=1
              checkcallout $curtry $zone $net $node $point
              sleep $dialdelay
	    fi
	  else
            checkcallout $curtry $zone $net $node $point
            sleep $dialdelay
          fi
        fi
      fi
    done
    rm $poll_file
  fi

}

function main ()
{

  cd $outb
  
  while [ ! -f /tmp/callout.exit ] ; do
    if [ ! -e ${lock}LCK..$TTY ]; then
      scandir
      sleep $scan
    else
      sleep $lockdelay
    fi
  done
  rm /tmp/callout.exit
}

if [ `whoami` != "$mailer_owner" ]; then
  echo "wrong uid, run as user $mailer_owner (rc=2)"
  exit 2
fi

. /etc/profile > /dev/null 2>&1
let MAXTIME=${MAXTIME}*60
main >> /var/log/ifmail/callout.log 2>&1

## Изменения

# Ver 1.1, 09 Jul 2000:
#
# Обнуление числа попыток после успешной сессии
#
# Не работал MAXTIME

# Ver 1.0, 08 Jul 2000:
#
# Первая стабильная версия
