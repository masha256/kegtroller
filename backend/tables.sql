CREATE TABLE `user` (
  `user_id` int(11) NOT NULL auto_increment,
  `card_id` varchar(32) NOT NULL default '',
  `email` varchar(255) NOT NULL,
  `date_added` datetime NOT NULL,
  `balance` float(6,2) NOT NULL default '0.00',
  `disabled` tinyint(4) NOT NULL default '0',
  PRIMARY KEY  (`user_id`),
  KEY `card_id` (`card_id`),
  KEY `email` (`email`)
);

CREATE TABLE `transaction` (
  `transaction_id` int(11) NOT NULL auto_increment,
  `user_id` int(11) NOT NULL,
  `date_added` datetime NOT NULL,
  `amount` float(6,2) NOT NULL default '0.00',
  `description` varchar(255) NOT NULL default '',
  PRIMARY KEY  (`transaction_id`),
  KEY `user_id` (`user_id`)
);

