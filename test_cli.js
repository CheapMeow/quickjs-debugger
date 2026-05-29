function add(a, b)
{
    let sum = a + b;
    let n = 0;
    while (n < 5000000)
        n = n + 1;
    return sum;
}

function main()
{
    let result = add(3, 4);
    let doubled = result * 2;
    let n = 0;
    while (n < 10000000)
        n = n + 1;
    return doubled;
}

main();
