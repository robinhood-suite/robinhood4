# com

python = import('python').find_installation('python3.9')

rbh_policy_consts = python.extension_module(
    'rbh_policy_consts',
    sources: ['rbh_policy_consts.c'],
    include_directories: include_directories('../../librobinhood/include'),
    dependencies: [python.dependency()],
    install: true,
    subdir: 'rbhpolicy'
)
