integration_tests = [
    'test_perm', 'test_xattr'
]

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e)
endforeach
