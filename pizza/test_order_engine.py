import unittest
from order_engine import Status
from order_engine import OrderParser
from order_engine import OrderEngine

class StatusTestCase(unittest.TestCase):
    """Tests for Status"""
    def setUp(self):
        Status.init()

    def testStatus(self):
        ss = ["NEW", "COOKING", "DELIVERING", "DELIVERED", "REFUNDED"]
        sCanceled = Status.getStatus('CANCELED')

        self.assertRaises(Exception, Status.getStatus, "SOMETHING")

        # asString
        for s in ss:
            self.assertEqual(Status.asString(Status.getStatus(s)), s)
        self.assertEqual(Status.asString(sCanceled), 'CANCELED')

        # Transfer
        for i in range(1, len(ss)-1):
            self.assertTrue(Status.isTransferrable(Status.getStatus(ss[i-1]),
                                          Status.getStatus(ss[i])))

        for i in range(0, 2):
            self.assertTrue(Status.isTransferrable(Status.getStatus(ss[i]), sCanceled))

        for i in range(3, len(ss)-1):
            self.assertTrue(not Status.isTransferrable(Status.getStatus(ss[i]), sCanceled))

        # isPaid
        for i in range(1, 4):
            self.assertTrue(Status.isPaid(Status.getStatus(ss[i])))

        self.assertTrue(not Status.isPaid(Status.getStatus('NEW')))
        self.assertTrue(not Status.isPaid(Status.getStatus('REFUNDED')))
        self.assertTrue(not Status.isPaid(sCanceled))
        
        # isNew
        self.assertTrue(Status.isNew(Status.new()))

class OrderParserTestCase(unittest.TestCase):
    """Tests for OrderParser"""
    def setUp(self):
        Status.init()
        self.parser = OrderParser(False)

    def testIsNonnegInt(self):
        self.assertTrue(OrderParser.isNonnegInt(5));
        self.assertTrue(not OrderParser.isNonnegInt(-1))
        self.assertTrue(not OrderParser.isNonnegInt("5"))

    def testParseOrder(self):
        # invalid json
        self.assertEqual(self.parser.parseOrder(0, '{a:a}'), None)
        # missing field
        self.assertEqual(self.parser.parseOrder(0, '{"orderId": 5}'), None)
        # type incorrect
        self.assertEqual(self.parser.parseOrder(0, 
            '{"orderId": 100, "status": "NEW", "updateId":"287", "amount": 20}'), None)
        # un-recognized status
        self.assertEqual(self.parser.parseOrder(0, 
            '{"orderId": 100, "status": "SOMETHING", "updateId":287, "amount": 20}'), None)
        # NEW with no amount
        self.assertEqual(self.parser.parseOrder(0, 
            '{"orderId": 100, "status": "NEW", "updateId":287}'), None)
        # extra fields
        o = self.parser.parseOrder(0, 
           '{"orderId": 100, "status": "NEW", "updateId":287, "amount": 20, "extra": 10}')
        self.assertTrue(o != None)
        self.assertEqual(o['status'], Status.new())
        self.assertEqual(o['orderId'], 100)
        self.assertEqual(o['updateId'], 287)
        self.assertEqual(o['amount'], 20)

class OrderEngineTestCase(unittest.TestCase):
    """ Tests for OrderEngine"""
    def setUp(self):
        Status.init()
        self.parser = OrderEngine()

    def getOrder(self, status, orderId, updateId):
        return {"status": Status.getStatus(status),
                "orderId": orderId,
                "updateId": updateId}

    def getNewOrder(self, orderId, updateId, amount):
        o = self.getOrder("NEW", orderId, updateId)
        o["amount"] = amount
        return o

    def testAddOrder(self):
        self.assertEqual(len(self.parser.records.keys()), 0)
        self.parser.addOrder(None)
        self.assertEqual(len(self.parser.records.keys()), 0)

        # cancel (1, 5)
        self.parser.addOrder(self.getOrder("CANCELED", 1, 5))
        self.assertEqual(len(self.parser.records.keys()), 0)

        # new (1, 5)
        self.parser.addOrder(self.getNewOrder(1, 5, 20))
        self.assertEqual(len(self.parser.records.keys()), 1)
        self.assertEqual(self.parser.records[1].status, Status.getStatus("NEW"))

        # cooking (1, 5) --- existing updateId
        self.parser.addOrder(self.getOrder("COOKING", 1, 5))
        self.assertEqual(len(self.parser.records.keys()), 1)
        self.assertEqual(self.parser.records[1].status, Status.getStatus("NEW"))

        # cooking (1, 2) --- non-increasing updateId
        self.parser.addOrder(self.getOrder("COOKING", 1, 2))
        self.assertEqual(len(self.parser.records.keys()), 1)
        self.assertEqual(self.parser.records[1].status, Status.getStatus("NEW"))

        # cooking (1, 6) --- ok
        self.parser.addOrder(self.getOrder("COOKING", 1, 6))
        self.assertEqual(len(self.parser.records.keys()), 1)
        self.assertEqual(self.parser.records[1].status, Status.getStatus("COOKING"))

        # delivered --- non-transferrable
        self.parser.addOrder(self.getOrder("DELIVERED", 1, 7))
        self.assertEqual(len(self.parser.records.keys()), 1)
        self.assertEqual(self.parser.records[1].status, Status.getStatus("COOKING"))

        # delivering (1, 7) --- ok
        self.parser.addOrder(self.getOrder("DELIVERING", 1, 7))
        self.assertEqual(len(self.parser.records.keys()), 1)
        self.assertEqual(self.parser.records[1].status, Status.getStatus("DELIVERING"))

        # cancle --- ok
        self.parser.addOrder(self.getOrder("CANCELED", 1, 8))
        self.assertEqual(len(self.parser.records.keys()), 1)
        self.assertEqual(self.parser.records[1].status, Status.getStatus("CANCELED"))

    def checkSummary(self, s, news=0, cookings=0, deliverings=0, delivereds=0,
            refundeds=0, cancels=0):
        self.assertEqual(s[Status.getStatus("NEW")], news)
        self.assertEqual(s[Status.getStatus("COOKING")], cookings)
        self.assertEqual(s[Status.getStatus("DELIVERING")], deliverings)
        self.assertEqual(s[Status.getStatus("DELIVERED")], delivereds)
        self.assertEqual(s[Status.getStatus("REFUNDED")], refundeds)
        self.assertEqual(s[Status.getStatus("CANCELED")], cancels)

    def testSummary(self):
        s = {}

        self.parser.addOrder(self.getNewOrder(1, 1, 5))
        self.parser.addOrder(self.getNewOrder(2, 2, 10))
        self.parser.addOrder(self.getNewOrder(3, 4, 5))
        total = self.parser.generateSummary(s)
        self.assertEqual(total, 0)
        self.checkSummary(s, 3)

        self.parser.addOrder(self.getOrder("COOKING", 2, 4));
        total = self.parser.generateSummary(s)
        self.assertEqual(total, 10)
        self.checkSummary(s, 2, 1)

        self.parser.addOrder(self.getOrder("DELIVERING", 2, 5));
        total = self.parser.generateSummary(s)
        self.assertEqual(total, 10)
        self.checkSummary(s, 2, 0, 1)

        self.parser.addOrder(self.getOrder("CANCELED", 2, 6));
        total = self.parser.generateSummary(s)
        self.assertEqual(total, 0)
        self.checkSummary(s, 2, 0, 0, 0, 0, 1)

if __name__ == '__main__':
        unittest.main()
