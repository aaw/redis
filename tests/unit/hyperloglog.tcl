start_server {tags {"hyperloglog"}} {

    # Since we're using 2^11 hash functions in our HyperLogLog implementation,
    # we expect this relative error, whis is about 2.3%. In tests that use this
    # value, we'll be a little more lenient because we expect this error with
    # high probability but there are input sequences that can cause the
    # relative error to go above this bound.

    set expected_relative_error [expr 1.04 / sqrt(pow(2,11))]

    test {KCARD returns 0 against non existing key} {
        r kcard no-key
    } 0

    test {repeated KADD with same key only counts one occurrence} {
        for {set i 0} {$i < 20} {incr i} {
            r kadd singleton unique-key
        }
        r kcard singleton
    } 1

    test {KADD can add multiple elements to a set in a single call} {
        r kadd multi-key-set a b c c d b a
        r kcard multi-key-set
    } 4

    test {KCARD returns exact cardinalities for small sets} {
        for {set i 1} {$i <= 10} {incr i} {
            r kadd smallset key$i
            assert_equal $i [r kcard smallset]
        }
    }

    test {KUNIONCARD gives exact cardinalities for small sets} {
        r kadd unionset1 abc
        r kadd unionset1 abcd
        r kadd unionset2 abc
        r kadd unionset2 abcde
        r kadd unionset3 abc
        r kadd unionset3 abcdef
        assert_equal 3 [r kunioncard unionset1 unionset2]
        assert_equal 3 [r kunioncard unionset2 unionset3]
        assert_equal 3 [r kunioncard unionset1 unionset3]
        assert_equal 4 [r kunioncard unionset1 unionset2 unionset3]
    }

    test {KUNIONCARD treats empty sets correctly} {
        r kadd kseta abc
        r kadd kseta abcd
        r kunioncard kseta ksetb
    } 2

    test {KINTERCARD gives exact cardinalities for small sets} {
        r kadd interset1 abc
        r kadd interset1 abcd
        r kadd interset2 abc
        r kadd interset2 abcde
        r kadd interset3 abcdef
        r kadd interset3 abcde
        assert_equal 1 [r kintercard interset1 interset2]
        assert_equal 1 [r kintercard interset2 interset3]
        assert_equal 0 [r kintercard interset1 interset3]
        assert_equal 0 [r kintercard interset1 interset2 interset3]
        r kadd interset3 abc
        assert_equal 1 [r kintercard interset1 interset2 interset3]
    }
    
    test {KINTERCARD treats empty sets correctly} {
        r kadd kseta abc
        r kintercard kseta ksetb ksetc
    } 0

    test {KUNIONCARD treats self-unions correctly} {
        r kadd selfunion abc
        r kadd selfunion def
        r kadd selfunion egh
        r kunioncard selfunion selfunion selfunion
    } 3

    test {KINTERCARD treats self-intersections correctly} {
        r kadd selfintersect abc
        r kadd selfintersect def
        r kadd selfintersect egh
        r kintercard selfintersect selfintersect selfintersect
    } 3

    test {KUNIONCARD can perform unions with many sets} {
        for {set i 0} {$i < 8} {incr i} {
            r kadd mu$i abc
            r kadd mu$i bcd
        }
        r kadd mu7 cde
        r kunioncard mu0 mu1 mu2 mu3 mu4 mu5 mu6 mu7
    } 3

    test {KINTERCARD can perform intersections with many sets} {
        for {set i 0} {$i < 8} {incr i} {
            r kadd mi$i abc
            r kadd mi$i bcd
        }
        r kadd mi7 cde
        r kintercard mi0 mi1 mi2 mi3 mi4 mi5 mi6 mi7
    } 2

    tags {"slow"} {
        
        test {KCARD returns approximate cardinalities for larger sets} {
            set bad_estimates 0
            set really_bad_estimates 0
            set num_tests 50000
            for {set i 0} {$i < $num_tests} {incr i} {
                r kadd largeset key:$i
                set relative_error [expr abs($i - [r kcard largeset] + 1)/double($i + 1)]
                if {$relative_error > $expected_relative_error} {incr bad_estimates}
                if {$relative_error > 6 * $expected_relative_error} {incr really_bad_estimates}
            }
            # There's nothing magic about these bounds, different hash functions and
            # different key sequences can yield different results, but the attempt is
            # to show that at every step along the way to 50000 elements we're getting
            # reasonable estimates.
            assert {$bad_estimates < $num_tests * 0.50}
            assert {$really_bad_estimates < $num_tests * 0.05}
        }
        
        test {KUNIONCARD returns approximate cardinalities for larger sets with small intersections} {
            for {set i 0} {$i < 20000} {incr i} {
                r kadd smalliunion1 key:$i
                r kadd smalliunion2 otherkey:$i
            }
            for {set i 0} {$i < 2000} {incr i} {
                r kadd smalliunion1 samekey:$i
                r kadd smalliunion2 samekey:$i
            }
            set relative_error_1 [expr abs(22000 - [r kcard smalliunion1])/22000.0]
            set relative_error_2 [expr abs(22000 - [r kcard smalliunion2])/22000.0]
            set relative_error_union [expr abs(42000 - [r kunioncard smalliunion1 smalliunion2])/42000.0]
            assert {$relative_error_1 < 3 * $expected_relative_error}
            assert {$relative_error_2 < 3 * $expected_relative_error}
            assert {$relative_error_union < 3 * $expected_relative_error}
        }
        
        test {KUNIONCARD returns approximate cardinalities for larger sets with large intersections} {
            for {set i 0} {$i < 2000} {incr i} {
                r kadd largeiunion1 key:$i
                r kadd largeiunion2 otherkey:$i
            }
            for {set i 0} {$i < 20000} {incr i} {
                r kadd largeiunion1 samekey:$i
                r kadd largeiunion2 samekey:$i
            }
            set relative_error_1 [expr abs(22000 - [r kcard largeiunion1])/22000.0]
            set relative_error_2 [expr abs(22000 - [r kcard largeiunion2])/22000.0]
            set relative_error_union [expr abs(24000 - [r kunioncard largeiunion1 largeiunion2])/24000.0]
            assert {$relative_error_1 < 3 * $expected_relative_error}
            assert {$relative_error_2 < 3 * $expected_relative_error}
            assert {$relative_error_union < 3 * $expected_relative_error}           
        }
        
        test {KINTERCARD returns approximate cardinalities for larger sets with small intersections} {
            for {set i 0} {$i < 20000} {incr i} {
                r kadd smalliinter1 key:$i
                r kadd smalliinter2 otherkey:$i
            }
            for {set i 0} {$i < 2000} {incr i} {
                r kadd smalliinter1 samekey:$i
                r kadd smalliinter2 samekey:$i
            }
            set relative_error_1 [expr abs(22000 - [r kcard smalliinter1])/22000.0]
            set relative_error_2 [expr abs(22000 - [r kcard smalliinter2])/22000.0]
            set relative_error_intersection [expr abs(2000 - [r kintercard smalliinter1 smalliinter2])/2000.0]
            assert {$relative_error_1 < 3 * $expected_relative_error}
            assert {$relative_error_2 < 3 * $expected_relative_error}
            assert {$relative_error_intersection < 3 * $expected_relative_error}            
        }
        
        test {KINTERCARD returns approximate cardinalities for larger sets with large intersections} {
            for {set i 0} {$i < 2000} {incr i} {
                r kadd largeiinter1 key:$i
                r kadd largeiinter2 otherkey:$i
            }
            for {set i 0} {$i < 20000} {incr i} {
                r kadd largeiinter1 samekey:$i
                r kadd largeiinter2 samekey:$i
            }
            set relative_error_1 [expr abs(22000 - [r kcard largeiinter1])/22000.0]
            set relative_error_2 [expr abs(22000 - [r kcard largeiinter2])/22000.0]
            set relative_error_intersection [expr abs(20000 - [r kintercard largeiinter1 largeiinter2])/20000.0]
            assert {$relative_error_1 < 3 * $expected_relative_error}
            assert {$relative_error_2 < 3 * $expected_relative_error}
            assert {$relative_error_intersection < 3 * $expected_relative_error}                    
        }

    }

}