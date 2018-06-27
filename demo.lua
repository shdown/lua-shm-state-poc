n = 0

function f()
    n = n + 1
    print('hello from f:', 'n = ' .. n, 'PID ' .. shm_poc.getpid())
    shm_poc.sleep(0.3)
end

function g()
    n = n + 1
    print('hello from g:', 'n = ' .. n, 'PID ' .. shm_poc.getpid())
    shm_poc.sleep(0.3)
end
