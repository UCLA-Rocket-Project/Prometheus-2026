

## Considerations

Requirements:
    
    1. Data has to be logged to an SD card, and logged out via Serial
    
    2. Once data is available on the radio's buffer, the buffer should be cleared immediately becuase overflowing the FiFo buffer of the LoRa may cause it to error out (not sure how we can recover from this as of now)

    3. To optimize for size, the incoming data will likely come in as bytes that have to be `sprintf`-ed so that they can be printed and logged properly

<hr />

In terms of data structure to inform the program to receive a message, there are a few that I think are worth considering:
    
    1. Task notifications - the most lightweight way to inform the logging task that we have received a message for processing, but this cannot meet the requirement to pass data along to other tasks, nor can multiple tasks read from this at once

    2. Esp-IDF ring buffer - decently optimized operation since you can read directly from the ring buffer instead of receiving by copy (esp-idf unique implementation, but should be similar to a message buffer)

    3. Queue - seems to be built for a scenario where there are multiple senders / receivers, but this is not what we need in this case


## Flow

ISR Task -> Decode & Serial Printing Task -> SD Card logging task