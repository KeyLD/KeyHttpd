#!/usr/bin/env ruby
require 'cgi'

cgi = CGI.new
puts cgi.header

printf "<html><body bgcolor=\"%s\">",cgi['color']
printf "<center><h1>Your chosen color is %s </h1></center>",cgi['color']
puts "</body></html>"
