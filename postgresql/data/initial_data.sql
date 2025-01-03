INSERT INTO hello_schema.users(name, count)
VALUES ('user-from-initial_data.sql', 42)
ON CONFLICT (name)
DO NOTHING;

INSERT INTO lavka.couriers VALUES (1, 'FOOT', ARRAY[80], ARRAY['[10:00, 12:00]'::lavka.timerange]) ON CONFLICT (id) DO NOTHING;
INSERT INTO lavka.couriers VALUES (2, 'AUTO', ARRAY[80, 90, 100], ARRAY['[12:00, 18:00]'::lavka.timerange]) ON CONFLICT (id) DO NOTHING;

INSERT INTO lavka.orders (id, regions, delivery_hours, cost, weight) VALUES (1, 80, ARRAY['[10:00,12:00]'::lavka.timerange], 500, 3) ON CONFLICT (id) DO NOTHING;
INSERT INTO lavka.orders (id, regions, delivery_hours, cost, weight) VALUES (2, 80, ARRAY['[10:00,12:00]'::lavka.timerange], 500, 1) ON CONFLICT (id) DO NOTHING;
INSERT INTO lavka.orders (id, regions, delivery_hours, cost, weight) VALUES (3, 80, ARRAY['[10:00,12:00]'::lavka.timerange], 500, 1) ON CONFLICT (id) DO NOTHING;
INSERT INTO lavka.orders (id, regions, delivery_hours, cost, weight) VALUES (4, 80, ARRAY['[10:00,12:00]'::lavka.timerange], 500, 1) ON CONFLICT (id) DO NOTHING;
INSERT INTO lavka.orders (id, regions, delivery_hours, cost, weight) VALUES (5, 80, ARRAY['[10:00,12:00]'::lavka.timerange], 500, 1) ON CONFLICT (id) DO NOTHING;
INSERT INTO lavka.orders (id, regions, delivery_hours, cost, weight) VALUES (6, 80, ARRAY['[10:00,12:00]'::lavka.timerange], 500, 1) ON CONFLICT (id) DO NOTHING;
INSERT INTO lavka.orders (id, regions, delivery_hours, cost, weight) VALUES (7, 80, ARRAY['[10:00,12:00]'::lavka.timerange], 500, 1) ON CONFLICT (id) DO NOTHING;
INSERT INTO lavka.orders (id, regions, delivery_hours, cost, weight) VALUES (8, 80, ARRAY['[10:00,12:00]'::lavka.timerange], 500, 1) ON CONFLICT (id) DO NOTHING;

/*INSERT INTO lavka.couriers VALUES (1, 'AUTO', ARRAY[1, 2, 3], ARRAY['[11:30,12:10]'::lavka.timerange, '[12:30,15:00]'::lavka.timerange]) ON CONFLICT (id) DO NOTHING;
INSERT INTO lavka.couriers VALUES (2, 'BIKE', ARRAY[2, 3], '{}') ON CONFLICT (id) DO NOTHING;
INSERT INTO lavka.couriers VALUES (3, 'FOOT', ARRAY[1], '{}') ON CONFLICT (id) DO NOTHING;*/

/*INSERT INTO lavka.couriers_assignments VALUES(1, '10:00:00', ARRAY[1, 2]);
INSERT INTO lavka.couriers_assignments VALUES(1, '10:35:00', ARRAY[3, 4]);
INSERT INTO lavka.couriers_assignments VALUES(1, '11:10:00', ARRAY[5, 6]);
INSERT INTO lavka.couriers_assignments VALUES(1, '11:45:00', ARRAY[7, 8]);*/
