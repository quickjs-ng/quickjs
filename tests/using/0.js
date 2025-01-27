function* gen() { yield {} }
async function* agen() { yield {} }
{
    using x = {}
    using y = {}, z = {}
}
{
    await using x = Promise.resolve({})
    await using y = Promise.resolve({}), z = Promise.resolve({})
}
{
    for (using x of gen()) {}
    for (await using x of gen()) {}
    for await (using x of agen()) {}
    for await (await using x of agen()) {}
}
