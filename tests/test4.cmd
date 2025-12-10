echo this tests variable substitution
roman=a_lol
myron=bigger_lol
complex=myron_is_${myron}_and_roman_is_${roman}
echo (${roman})! who is ${myron}?
echo ${complex} | grep is
echo {${myron}}, fishy ${complex}