#!/bin/bash
#touch roots.pem
echo '' > roots.pem

for filename in *.0
do

#  is_amazon=$(openssl x509 -in $filename -text -nocert | grep "O = Amazon")
  is_google=$(openssl x509 -in $filename -text -nocert | grep "O = Google Trust Services LLC")
  is_comodo=$(openssl x509 -in $filename -text -nocert | grep "O = Comodo CA Limited")
  is_digicert=$(openssl x509 -in $filename -text -nocert | grep "O = DigiCert")
  is_isrg=$(openssl x509 -in $filename -text -nocert | grep "O = Internet Security Research Group")
  is_verisign=$(openssl x509 -in $filename -text -nocert | grep "O = \"VeriSign, Inc.\"")
  is_baltimore=$(openssl x509 -in $filename -text -nocert | grep "O = Baltimore")
  is_globalsign=$(openssl x509 -in $filename -text -nocert | grep "O = GlobalSign")
  is_starfield=$(openssl x509 -in $filename -text -nocert | grep "O = \"Starfield Technologies, Inc.\"")
  is_dst=$(openssl x509 -in $filename -text -nocert | grep "O = Digital Signature Trust Co.")
  is_entrust=$(openssl x509 -in $filename -text -nocert | grep "O = \"Entrust, Inc.\"")
  is_geotrust=$(openssl x509 -in $filename -text -nocert | grep "O = GeoTrust Inc.")
  is_godaddy=$(openssl x509 -in $filename -text -nocert | grep "O = \"GoDaddy.com, Inc.\"")
  is_cybertrust=$(openssl x509 -in $filename -text -nocert | grep "O = \"Cybertrust, Inc\"")

#  if [ ! -z "$is_amazon" ]
#  then
#    echo $is_amazon
#    openssl x509 -in $filename -text -certopt no_header,no_pubkey,no_subject,no_issuer,no_signame,no_version,no_serial,no_validity,no_extensions,no_sigdump,no_aux,no_extensions >> roots.pem
#  fi

  if [ ! -z "$is_google" ]
  then
    echo $is_google
    openssl x509 -in $filename -text -certopt no_header,no_pubkey,no_subject,no_issuer,no_signame,no_version,no_serial,no_validity,no_extensions,no_sigdump,no_aux,no_extensions >> roots.pem
  fi

  if [ ! -z "$is_comodo" ]
  then
    echo $is_comodo
    openssl x509 -in $filename -text -certopt no_header,no_pubkey,no_subject,no_issuer,no_signame,no_version,no_serial,no_validity,no_extensions,no_sigdump,no_aux,no_extensions >> roots.pem
  fi

  if [ ! -z "$is_digicert" ]
  then
    echo $is_digicert
    openssl x509 -in $filename -text -certopt no_header,no_pubkey,no_subject,no_issuer,no_signame,no_version,no_serial,no_validity,no_extensions,no_sigdump,no_aux,no_extensions >> roots.pem
  fi

  if [ ! -z "$is_isrg" ]
  then
    echo $is_isrg
    openssl x509 -in $filename -text -certopt no_header,no_pubkey,no_subject,no_issuer,no_signame,no_version,no_serial,no_validity,no_extensions,no_sigdump,no_aux,no_extensions >> roots.pem
  fi

  if [ ! -z "$is_verisign" ]
  then
    echo $is_verisign
    openssl x509 -in $filename -text -certopt no_header,no_pubkey,no_subject,no_issuer,no_signame,no_version,no_serial,no_validity,no_extensions,no_sigdump,no_aux,no_extensions >> roots.pem
  fi

  if [ ! -z "$is_baltimore" ]
  then
    echo $is_baltimore
    openssl x509 -in $filename -text -certopt no_header,no_pubkey,no_subject,no_issuer,no_signame,no_version,no_serial,no_validity,no_extensions,no_sigdump,no_aux,no_extensions >> roots.pem
  fi

  if [ ! -z "$is_globalsign" ]
  then
    echo $is_globalsign
    openssl x509 -in $filename -text -certopt no_header,no_pubkey,no_subject,no_issuer,no_signame,no_version,no_serial,no_validity,no_extensions,no_sigdump,no_aux,no_extensions >> roots.pem
  fi

  if [ ! -z "$is_starfield" ]
  then
    echo $is_starfield
    openssl x509 -in $filename -text -certopt no_header,no_pubkey,no_subject,no_issuer,no_signame,no_version,no_serial,no_validity,no_extensions,no_sigdump,no_aux,no_extensions >> roots.pem
  fi

  if [ ! -z "$is_dst" ]
  then
    echo $is_dst
    openssl x509 -in $filename -text -certopt no_header,no_pubkey,no_subject,no_issuer,no_signame,no_version,no_serial,no_validity,no_extensions,no_sigdump,no_aux,no_extensions >> roots.pem
  fi

  if [ ! -z "$is_entrust" ]
  then
    echo $is_entrust
    openssl x509 -in $filename -text -certopt no_header,no_pubkey,no_subject,no_issuer,no_signame,no_version,no_serial,no_validity,no_extensions,no_sigdump,no_aux,no_extensions >> roots.pem
  fi

  if [ ! -z "$is_geotrust" ]
  then
    echo $is_geotrust
    openssl x509 -in $filename -text -certopt no_header,no_pubkey,no_subject,no_issuer,no_signame,no_version,no_serial,no_validity,no_extensions,no_sigdump,no_aux,no_extensions >> roots.pem
  fi

  if [ ! -z "$is_godaddy" ]
  then
    echo $is_godaddy
    openssl x509 -in $filename -text -certopt no_header,no_pubkey,no_subject,no_issuer,no_signame,no_version,no_serial,no_validity,no_extensions,no_sigdump,no_aux,no_extensions >> roots.pem
  fi

  if [ ! -z "$is_cybertrust" ]
  then
    echo $is_cybertrust
    openssl x509 -in $filename -text -certopt no_header,no_pubkey,no_subject,no_issuer,no_signame,no_version,no_serial,no_validity,no_extensions,no_sigdump,no_aux,no_extensions >> roots.pem
  fi

done

