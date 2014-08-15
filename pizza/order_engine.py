#!/usr/bin/env python

import fileinput
import json

class Status:
    """
    Encapsulate staus:
     - map btw a numbered status to a human-readable status
     - determine status transferrability
     - help functions for special status, like new; and special properties,
       like isPaid
    """
    STATUSES= {};

    @staticmethod
    def init():
        if(len(Status.STATUSES.keys()) != 0):
            return
        Status.STATUSES['CANCELED'] = -1
        for i, s in enumerate(["NEW", "COOKING", "DELIVERING", "DELIVERED", "REFUNDED"]):
            Status.STATUSES[s] = i

    @staticmethod
    def new():
        return Status.STATUSES['NEW']

    @staticmethod
    def getStatus(s):
        if(s not in Status.STATUSES):
            raise Exception("unrecognized status: " + s)
        return Status.STATUSES[s]

    # whether the status old can transfer to new
    # the status go: NEW -> COOKING -> DELIVERING -> DELIVERED -> REFUNDED
    #                 |         |         | -> CANCELED
    @staticmethod
    def isTransferrable(old, new):
        if(old == Status.STATUSES['CANCELED']):
            return False
        if(new == Status.STATUSES['CANCELED']):
            return old >= Status.STATUSES['NEW'] and old <= Status.STATUSES['DELIVERING']
        return new == old + 1

    # charged only after NEW, and has not been canceled or refunded.
    # used by OrderEngine.generateSummary
    @staticmethod
    def isPaid(i):
        return i > Status.STATUSES['NEW'] and i != Status.STATUSES['REFUNDED']

    @staticmethod
    def isNew(i):
        return i == Status.STATUSES['NEW']

    # TODO: When we have a lot statuses, then this should be implemented by 
    # adding a num->string map
    @staticmethod
    def asString(i):
        for k, v in Status.STATUSES.iteritems():
            if(v == i):
                return k
        return None


class OrderParser:
    """
     Parse the json input, ignore extra field, ignore(or report error) invalid input.
     - order:= orderId(sint), updateId(sint), status[, amount(sint)]
     - invalid are:
        - NEW with no amount
        - type of the field is not correct
        - status is not recognized
    """
    def __init__(self, reportErr=False):
        self.reportErr = reportErr

    @staticmethod
    def isNonnegInt(i):
        try:
            j = i + 1;
        except TypeError:
            return False;
        return i >= 0;

    def Err(self, no, e):
        if(self.reportErr):
            print "ParseError in line ", no, ": ", e

    def parseOrder(self, no, s):
        """
        Returns the raw-object which is of the type dictionary for the valid
        input, otherwise returns None

        arg no: the line number, used to report error
        arg s: the input json string
        """
        try:
            raw_obj = json.loads(s);
        except Exception, e:
            self.Err(no, e)
            return None

        if(("orderId" not in raw_obj)
                or ("updateId" not in raw_obj)
                or ("status" not in raw_obj)):
            self.Err(no, "orderId, updateId, and status are required.")
            return None

        if(not OrderParser.isNonnegInt(raw_obj['orderId'])
                or not OrderParser.isNonnegInt(raw_obj['updateId'])):
            self.Err(no, "orderId and updateId should be non-negative integers.")
            return None

        try:
            raw_obj['status'] = Status.getStatus(raw_obj['status']);
        except Exception, e:
            self.Err(no, e)
            return None

        if(Status.isNew(raw_obj['status'])):
            if('amount' not in raw_obj):
                self.Err(no, "amount is required for a NEW order.")
                return None
            if(not OrderParser.isNonnegInt(raw_obj['amount'])):
                self.Err(no, "amount should be an non-negative integer.");
                return None

        return raw_obj;

class OrderEngine:
    """
     process orders according to the business rules:
     - Each update identified by (orderId, updateId) is recognized only once and
       the updateId is increasing
     - For an order identified by orderId, its status transfers as specified in the
       doc
    """
    class Order:
        def __init__(self, orderId, updateId, amount):
            self.status = Status.new()
            self.amount = amount
            self.orderId = orderId
            self.lastUpdateId = updateId
            self.updateIds = set([updateId])

        def getNext(self):
            return self.status + 1;

        def addUpdate(self, updateId):
            self.updateIds.add(updateId)
            self.lastUpdateId = updateId

        def isValidUpdate(self, updateId):
            return updateId not in self.updateIds and updateId > self.lastUpdateId


    def __init__(self):
        self.records = {}

    def addOrder(self, o):
        """
        Process unique, transferrable orders
        Assume: the o is checked(generated) through OrderParser
        """
        if(o == None):
            return;

        if(o['orderId'] not in self.records):
            if(Status.isNew(o['status'])):
                self.records[o['orderId']] = OrderEngine.Order(o['orderId'],
                        o['updateId'], o['amount'])
            return

        ro = self.records[o['orderId']];
        if(ro.isValidUpdate(o['updateId']) 
                and Status.isTransferrable(ro.status, o['status'])):
            ro.addUpdate(o['updateId'])
            ro.status = o['status']

    # Fill summary and return total-amount-charged
    def generateSummary(self, summary):
        summary.clear()
        for v in Status.STATUSES.values():
            summary[v] = 0
        total = 0;

        for k, v in self.records.iteritems():
            summary[v.status] += 1;
            if(Status.isPaid(v.status)):
                total += v.amount
        return total

    # New:
    # Cooking:
    # Delivering:
    # Delivered:
    # Canceled:
    # Refunded:
    # Total amount charged
    def printSummary(self):
        summary = {}
        total = self.generateSummary(summary) 

        for k, v in summary.iteritems():
            print Status.asString(k).title(), ": ", v

        print "Table Amount Charged: ", total

if __name__ == '__main__':
    Status.init();
    order_proc = OrderParser(True);
    order_eng = OrderEngine()

    for line in fileinput.input():
        order_eng.addOrder(order_proc.parseOrder(fileinput.filelineno(), line))

    order_eng.printSummary()
