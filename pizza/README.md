# Pete's Pizza Order-Engine Specification

## Goals
Given the input as lines of json describing the update-order, output the
summary of New, Cooking, Delivering, Delivered, Refunded, Canceled, Total
Amount Charged.

## Rules

### Input rules

- update-order := orderId(sint), updateId(sint), status[, amount(sint)]
- status := NEW | COOKING | DELIVERING | DELIVERED | REFUNDED | CANCELED
- amount is required for the NEW

### Order rules

- for an order specified by orderId, it starts with NEW and the updateIds should 
  be increasingly unique
- Charged after NEW; CANCELED and REFUNDED will return the money

### Status rules

- status can transfer as following:
<pre>
     the status go: NEW -> COOKING -> DELIVERING -> DELIVERED -> REFUNDED
                     |         |         | -> CANCELED
<pre>

# Implementation:

- class Status:
    - map btw human-readable status to numbered status, see init() and
      asString()
    - determine whether an old status can transfer to a new one, see
      isTransferrable()
    - help functions about special status and special properties

- class OrderParser
    - parse json, and recognize the valid input. Conditionally(based on the
      property reportErr) report error

- class OrderEngine
    - process the parsed(from OrderParser) update-order, make sure the "Order
      rules" are followed

# tests:

- StatusTestCase
    - test status transfer
    - test status properties, asString
- OrderParserTestCase
    - test isNonnegInt
    - test parse bad and good input
- OrderEngineTestCase
    - test add order: duplicate, non-increasing, un-transferrable, ok
    - test summary
