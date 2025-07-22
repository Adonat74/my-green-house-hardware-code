# my-green-house-hardware-code

> This is the hardware code for the my green house captors

## Routes(text) to request via bluetooth the datas stored inside the sd card.

- "GET /last" :
retrieve the last datas received and stored into the sd card.

- "GET /avg/day/{date}" {date} format example = "2025-07-04" :
retrieve the average values of a given day.

- "GET /avg/week/{week}" {week} format example = "27" :
retrieve the average values of a given week -> week 27 of the year.

- "GET /avg/month/{month}" {month} format example = "07" :
retrieve the average values of a given month -> July

- "GET /all/day/{date}" {date} format example = "2025-07-04" :
retrieve all values of a given day

- "GET /all/week" :
retrieve all weeks average values

- "GET /all/month" :
retrieve all months average values
