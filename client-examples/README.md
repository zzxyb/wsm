# Brightness client example

Build the project, then run the example against a running WSM session:

```sh
./build/client-examples/wsm-brightness-client
```

Print one output:

```sh
./build/client-examples/wsm-brightness-client --output eDP-1
```

Set its brightness:

```sh
./build/client-examples/wsm-brightness-client \
  --output eDP-1 --brightness 50
```

The brightness value uses the raw range reported by `min_brightness` and
`max_brightness`.
