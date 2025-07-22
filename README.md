# my-green-house-hardware-code

> This is the hardware code for the my green house captors

## Routes(text) to request via bluetooth the datas stored inside the sd card.

- "GET /last" :

retrieve the last datas received and stored into the sd card.
Return format :    "2025-07-22 16:32:11 H:49.45 T:27.00 S:576"

- "GET /avg/day/{date}" {date} format example = "2025-07-03" :

retrieve the average values of a given day.
Return format :    "AVG H:49.45 T:27.00 S:576"


- "GET /avg/week/{week}" {week} format example = "27" :

retrieve the average values of a given week -> week 27 of the year.
Return format :    "2025-W27 H:49.45 T:27.00 S:576"


- "GET /avg/month/{month}" {month} format example = "06" :

retrieve the average values of a given month -> July
Return format :    "2025-06 AVG H:49.45 T:27.00 S:576"


> when retrieving the days, weeks and months all values it will just return multiple strings with the format seen above

- "GET /all/day/{date}" {date} format example = "2025-07-04" :

retrieve all values of a given day

- "GET /all/week" :

retrieve all weeks average values

- "GET /all/month" :

retrieve all months average values
