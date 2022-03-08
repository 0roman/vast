module "network" {
  source = "./network"

  peered_vpc_id = var.vpc_id
  subnet_cidr   = var.subnet_cidr

  providers = {
    aws               = aws
    aws.monitored_vpc = aws.monitored_vpc
  }
}

module "vast_server" {
  source = "./fargate"

  name        = "vast-server"
  region_name = var.region_name

  vpc_id                      = var.vpc_id
  subnet_id                   = module.network.private_subnet_id
  ingress_subnet_cidrs        = [module.network.private_subnet_cidr]
  ecs_cluster_id              = aws_ecs_cluster.fargate_cluster.id
  ecs_cluster_name            = aws_ecs_cluster.fargate_cluster.name
  ecs_task_execution_role_arn = aws_iam_role.fargate_task_execution_role.arn

  task_cpu    = 2048
  task_memory = 4096

  docker_image        = "${module.env.vast_server_image}:${var.vast_version}"
  storage_type        = var.vast_server_storage_type
  storage_mount_point = "/var/lib/vast"

  command = ["-e", "0.0.0.0:42000", "start"]
  port    = 42000

  environment = [{
    name  = "AWS_REGION"
    value = var.region_name
  }]
}

module "vast_client" {
  source = "./lambda"

  function_base_name = "client"
  region_name        = var.region_name
  docker_image       = "${aws_ecr_repository.lambda.repository_url}:${time_static.last_image_upload.unix}"
  memory_size        = 2048
  timeout            = 300

  in_vpc  = true
  vpc_id  = module.network.vast_vpc_id
  subnets = [module.network.private_subnet_id]

  additional_policies = []
  environment         = {}

  depends_on = [
    null_resource.lambda_image_push
  ]
}
