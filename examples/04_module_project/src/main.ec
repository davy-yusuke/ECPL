import "math"

fn main() {
    sum: i32 := add(10, 20)
    println(sum)
    
    product: i32 := mul(5, 6)
    println(product)

    sum = sub(product, 10)

    println(sum)

    result: i32 := div(sum, 2)

    println(result)
}
