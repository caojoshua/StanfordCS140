# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(syn-write) begin
(syn-write) create "stuff"
(syn-write) exec child 1 of 10: "child-syn-wrt 0"
(syn-write) exec child 2 of 10: "child-syn-wrt 1"
(syn-write) exec child 3 of 10: "child-syn-wrt 2"
(syn-write) exec child 4 of 10: "child-syn-wrt 3"
(syn-write) exec child 5 of 10: "child-syn-wrt 4"
(syn-write) exec child 6 of 10: "child-syn-wrt 5"
(syn-write) exec child 7 of 10: "child-syn-wrt 6"
(syn-write) exec child 8 of 10: "child-syn-wrt 7"
(syn-write) exec child 9 of 10: "child-syn-wrt 8"
(syn-write) exec child 10 of 10: "child-syn-wrt 9"
(syn-write) open "stuff"
(syn-write) read "stuff"
(syn-write) end
EOF
pass;
