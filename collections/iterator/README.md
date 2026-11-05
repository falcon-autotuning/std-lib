# falcon/collections/iterator

Generic sequential traversal for Falcon arrays.

`Iterator<T>` maintains a position within an `Array<T>` and provides routines
for moving forward and backward through the collection while evaluating the
element at the current position.

---

## Installation

```fal
import "libs/collections/iterator/iterator.fal";
```

In your `falcon.yml`:

```yaml
dependencies:
  - iterator
```

---

## Type

```fal
struct Iterator<T> { ... }
```

`T` may be any primitive or struct type.

Examples:

```fal
Iterator<int> numbers = Iterator.New(values);
Iterator<string> words = Iterator.New(words);
Iterator<MyStruct> items = Iterator.New(items);
```

---

## Routines

### `New(Array<T> contents) -> (Iterator<T> it)`

Creates a new iterator over the provided array.

The iterator is initially positioned at the first element (index `0`).

```fal
Array<int> values = Array.New();
values.PushBack(10);
values.PushBack(20);

Iterator<int> it = Iterator.New(values);
```

---

### `Beginning() -> ()`

Moves the iterator to the first element of the array.

```fal
it.Beginning();
```

---

### `End() -> ()`

Moves the iterator to the final element of the array.

```fal
it.End();
```

---

### `CurrentIndex() -> (int index)`

Returns the iterator's current position.

```fal
int idx = it.CurrentIndex();
```

---

### `HasNext() -> (bool has_next)`

Returns `true` if the iterator can advance to a later element.

```fal
if (it.HasNext()) {
    bool moved = it.Increment();
}
```

---

### `HasPrevious() -> (bool has_previous)`

Returns `true` if the iterator can move to an earlier element.

```fal
if (it.HasPrevious()) {
    bool moved = it.Decrement();
}
```

---

### `Increment() -> (bool success)`

Advances the iterator to the next element.

Returns:

- `true` if the move occurred.
- `false` if the iterator is already positioned at the final element.

```fal
bool moved = it.Increment();
```

---

### `Decrement() -> (bool success)`

Moves the iterator to the previous element.

Returns:

- `true` if the move occurred.
- `false` if the iterator is already positioned at the first element.

```fal
bool moved = it.Decrement();
```

---

### `Evaluate() -> (T value)`

Returns the element at the iterator's current position.

```fal
int value = it.Evaluate();
```

For user-defined types:

```fal
TestPoint point = it.Evaluate();
```

---

## Navigation Pattern

Forward traversal:

```fal
Iterator<int> it = Iterator.New(values);

int value = it.Evaluate();

while (it.HasNext()) {
    bool moved = it.Increment();
    value = it.Evaluate();
}
```

Backward traversal:

```fal
Iterator<int> it = Iterator.New(values);

it.End();

int value = it.Evaluate();

while (it.HasPrevious()) {
    bool moved = it.Decrement();
    value = it.Evaluate();
}
```

---

## Generic Types

`Iterator<T>` supports primitive and user-defined types.

Primitive example:

```fal
Array<int> values = Array.New();
values.PushBack(10);
values.PushBack(20);

Iterator<int> it = Iterator.New(values);

int value = it.Evaluate();
```

Struct example:

```fal
struct TestPoint {
    int x_;
    int y_;
}

Array<TestPoint> points = Array.New();

TestPoint p;
p.x_ = 1;
p.y_ = 2;

points.PushBack(p);

Iterator<TestPoint> it = Iterator.New(points);

TestPoint point = it.Evaluate();
```

---

## Design Notes

`Iterator<T>` never moves outside the valid bounds of the underlying array.

- `Increment()` fails rather than advancing beyond the final element.
- `Decrement()` fails rather than moving before the first element.
- `HasNext()` and `HasPrevious()` can be used to determine whether a
  navigation operation is possible before attempting it.

This allows traversal logic to avoid out-of-bounds runtime errors while
remaining simple and explicit.

---

## Running the Tests

```bash
falcon-test libs/collections/iterator/tests/run_tests.fal
```

---

## Full Example

```fal

autotuner IteratorExample -> (int value) {
    value = 0;

    start -> run;

    state run {
        Array<int> numbers = Array.New();

        numbers.PushBack(10);
        numbers.PushBack(20);
        numbers.PushBack(30);

        Iterator<int> it = Iterator.New(numbers);

        it.Increment();

        value = it.Evaluate(); // 20

        terminal;
    }
}
```
